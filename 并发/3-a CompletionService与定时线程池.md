---
title: "3-a CompletionService与定时线程池"
category: 并发编程
tags:
  - conc/pool
  - concurrency
difficulty: 深入
source: "JCiP / Java并发编程的艺术"
link:
  - "[[3-1 线程池核心理论与执行流程|线程池核心理论]]"
  - "[[3-3 线程池关闭与调优|线程池关闭与调优]]"
skills:
  - obsidian-markdown
---
## 🧩 模块7：CompletionService与批量任务并行化设计（对应JCiP 6.3.5；📗 艺术 10.4）【思想+核心源码】
### 7.1 核心维度定义
📘 JCiP原文核心定义：CompletionService将Executor与BlockingQueue融合，实现「批量任务按完成顺序获取结果」，解决了Future轮询获取结果的CPU空转问题，核心是重写FutureTask的done()方法，任务完成后自动将Future入队，与阶段2的阻塞队列完全联动。

### 7.2 核心设计思想
1.  **结果自动入队**：任务完成后自动进入完成队列，无需手动轮询所有Future；
2.  **按完成顺序获取**：take()/poll()按任务完成顺序返回，而非提交顺序，提升处理效率；
3.  **资源解耦**：Executor负责任务执行，BlockingQueue负责结果存储，职责分离；
4.  **异常隔离**：单个任务异常不影响其他任务的结果获取，异常在get()时抛出。

### 7.3 核心源码实现
```java
// 📘 JCiP Listing 6.15 CompletionService核心使用示例
public class CompletionServiceDemo {
    // 批量任务按完成顺序获取结果，实现页面渲染图片下载完成即显示
    public void renderPage(CharSequence source) {
        // 扫描页面中的所有图片信息
        List<ImageInfo> imageInfos = scanForImageInfo(source);
        // 创建线程池
        ExecutorService executor = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors() * 2);
        // 封装Executor为CompletionService
        CompletionService<ImageData> cs = new ExecutorCompletionService<>(executor);

        // 1. 提交所有图片下载任务
        for (ImageInfo imageInfo : imageInfos) {
            cs.submit(() -> imageInfo.downloadImage());
        }

        // 2. 先渲染页面文本
        renderText(source);

        // 3. 按完成顺序获取图片，下载完成一张渲染一张
        try {
            for (int i = 0; i < imageInfos.size(); i++) {
                // take()阻塞等待第一个完成的任务，按完成顺序返回
                Future<ImageData> completedFuture = cs.take();
                ImageData imageData = completedFuture.get();
                // 渲染单张图片
                renderImage(imageData);
            }
        } catch (InterruptedException e) {
            // 响应中断，恢复中断标记
            Thread.currentThread().interrupt();
        } catch (ExecutionException e) {
            // 处理单张图片下载异常，不影响其他图片
            e.printStackTrace();
        } finally {
            // 关闭线程池
            executor.shutdown();
        }
    }

    // 以下为模拟方法
    private List<ImageInfo> scanForImageInfo(CharSequence source) {
        return new ArrayList<>();
    }
    private void renderText(CharSequence source) {}
    private void renderImage(ImageData data) {}

    // 模拟实体类
    interface ImageInfo {
        ImageData downloadImage();
    }
    interface ImageData {}
}
```

### 7.4 ExecutorCompletionService底层实现原理
```java
// JDK8 ExecutorCompletionService核心实现
public class ExecutorCompletionService<V> implements CompletionService<V> {
    private final Executor executor;
    private final AbstractExecutorService aes;
    // 完成队列，存储已完成的任务Future
    private final BlockingQueue<Future<V>> completionQueue;

    // 继承FutureTask，重写done()方法，任务完成后自动入队
    private class QueueingFuture extends FutureTask<Void> {
        private final Future<V> task;
        QueueingFuture(RunnableFuture<V> task) {
            super(task, null);
            this.task = task;
        }
        // 任务完成后调用，自动将Future加入完成队列
        protected void done() { completionQueue.add(task); }
    }

    // 提交任务时，封装为QueueingFuture
    public Future<V> submit(Callable<V> task) {
        if (task == null) throw new NullPointerException();
        RunnableFuture<V> f = newTaskFor(task);
        executor.execute(new QueueingFuture(f));
        return f;
    }

    // 从完成队列获取结果
    public Future<V> take() throws InterruptedException {
        return completionQueue.take();
    }

    public Future<V> poll() {
        return completionQueue.poll();
    }

    public Future<V> poll(long timeout, TimeUnit unit) throws InterruptedException {
        return completionQueue.poll(timeout, unit);
    }
}
```

---
## 🧩 模块8：ScheduledThreadPoolExecutor核心设计（对应JCiP 6.2.5；📗 艺术 10.5）【思想+核心源码】
### 8.1 核心维度定义
📘 JCiP原文核心定义：ScheduledThreadPoolExecutor是支持定时/周期任务执行的线程池实现，继承ThreadPoolExecutor，基于DelayQueue实现任务的延迟执行，替代Timer类，解决了Timer单线程、异常崩溃、系统时钟敏感的缺陷，与阶段2的DelayQueue完全联动。

### 8.2 核心行为语义
1.  **schedule()**：延迟指定时间后执行一次性任务；
2.  **scheduleAtFixedRate()**：固定频率执行，以上次任务**开始时间**为基准，任务执行时间超过周期会导致后续任务累积延迟；
3.  **scheduleWithFixedDelay()**：固定延迟执行，以上次任务**结束时间**为基准，执行时间不影响周期，延迟稳定；
4.  任务执行抛出未捕获异常时，会取消该任务，不影响其他定时任务的执行；
5.  线程池关闭后，定时任务会根据设置的`continueExistingPeriodicTasksAfterShutdown`/`executeExistingDelayedTasksAfterShutdown`策略，决定是否继续执行。

### 8.3 核心源码实现
```java
// 📘 JCiP 6.2.5 定时任务核心示例
public class ScheduledExecutorDemo {
    public static void main(String[] args) throws InterruptedException {
        // 创建定时线程池，核心线程数2
        ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(2);

        // 1. 延迟1秒执行一次性任务
        scheduler.schedule(() -> {
            System.out.println("一次性延迟任务执行：" + System.currentTimeMillis());
        }, 1, TimeUnit.SECONDS);

        // 2. 固定频率执行：延迟1秒，每3秒执行一次（以上次开始时间为基准）
        System.out.println("固定频率任务开始：" + System.currentTimeMillis());
        ScheduledFuture<?> rateFuture = scheduler.scheduleAtFixedRate(() -> {
            System.out.println("固定频率执行-开始：" + System.currentTimeMillis());
            try {
                // 任务执行2秒，超过周期3秒的一半，下次执行仍按开始时间+3秒
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                System.out.println("固定频率任务被中断");
                return;
            }
            System.out.println("固定频率执行-结束：" + System.currentTimeMillis());
        }, 1, 3, TimeUnit.SECONDS);

        // 3. 固定延迟执行：延迟1秒，每次执行结束后延迟3秒执行
        System.out.println("固定延迟任务开始：" + System.currentTimeMillis());
        ScheduledFuture<?> delayFuture = scheduler.scheduleWithFixedDelay(() -> {
            System.out.println("固定延迟执行-开始：" + System.currentTimeMillis());
            try {
                // 任务执行2秒，下次执行在结束后+3秒
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                System.out.println("固定延迟任务被中断");
                return;
            }
            System.out.println("固定延迟执行-结束：" + System.currentTimeMillis());
        }, 1, 3, TimeUnit.SECONDS);

        // 运行10秒后取消任务，关闭线程池
        Thread.sleep(10000);
        rateFuture.cancel(true);
        delayFuture.cancel(true);
        scheduler.shutdown();
    }
}
```

---
