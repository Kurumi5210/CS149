# 作业 2：从头构建一个任务执行库

**截止日期：10 月 24 日（周四），晚上 11:59**

**总分：100 分**

## 概述

每个人都希望能快速完成任务，在本次作业中，我们就要求你做到这一点！你需要实现一个 C++ 库，该库能在多核 CPU 上尽可能高效地执行应用程序提供的任务。

在作业的第一部分，你要实现一个任务执行库版本，该版本支持批量（数据并行）启动同一任务的多个实例。此功能类似于你在作业 1 中用于跨核心并行化代码的 [ISPC 任务启动行为](http://ispc.github.io/ispc.html#task-parallelism-launch-and-sync-statements)。

在作业的第二部分，你要扩展任务运行时系统，以执行更复杂的 _任务图_，其中任务的执行可能依赖于其他任务产生的结果。这些依赖关系会限制你的任务调度系统可以安全并行运行哪些任务。在并行机器上调度数据并行任务图的执行是许多流行的并行运行时系统的一项特性，这些系统涵盖从流行的 [线程构建块](https://github.com/intel/tbb) 库，到 [Apache Spark](https://spark.apache.org/)，再到诸如 [PyTorch](https://pytorch.org/) 和 [TensorFlow](https://www.tensorflow.org/) 等现代深度学习框架。

本次作业要求你做到以下几点：
* 使用线程池管理任务执行
* 使用诸如互斥锁和条件变量等同步原语来协调工作线程的执行
* 实现一个能反映任务图所定义依赖关系的任务调度器
* 理解工作负载特性，以便做出高效的任务调度决策

### 等等，我觉得我之前做过类似的事？
你可能已经在诸如 CS107 或 CS111 等课程中创建过线程池和任务执行库。不过，本次作业是一个更好地理解这些系统的绝佳机会。你将实现多个任务执行库，有些不使用线程池，有些则使用不同类型的线程池。通过实现多种任务调度策略，并比较它们在不同工作负载下的性能，你将更好地理解创建并行系统时关键设计选择的影响。

## 环境设置
**我们将在亚马逊 AWS 的 `c7g.4xlarge` 实例上对本次作业进行评分——我们在 [这里](https://github.com/stanford-cs149/asst2/blob/master/cloud_readme.md) 提供了设置虚拟机的说明。请确保你的代码能在该虚拟机上正常运行，因为我们将使用它进行性能测试和评分。**

作业的起始代码可在 [Github](https://github.com/stanford-cs149/asst2) 上获取。请从以下链接下载作业 2 的起始代码：
```
https://github.com/stanford-cs149/asst2/archive/refs/heads/master.zip
```
**重要提示：** 请勿修改提供的 `Makefile`，否则可能会导致我们的评分脚本无法正常工作。

## 第一部分：同步批量任务启动
在作业 1 中，你使用 ISPC 的任务启动原语来启动 N 个 ISPC 任务实例（`launch[N] myISPCFunction()`）。在本次作业的第一部分，你要在任务执行库中实现类似的功能。

首先，熟悉 `itasksys.h` 中 `ITaskSystem` 的定义。这个 [抽象类](https://www.tutorialspoint.com/cplusplus/cpp_interfaces.htm) 定义了你的任务执行系统的接口。该接口有一个 `run()` 方法，其签名如下：
```
virtual void run(IRunnable* runnable, int num_total_tasks) = 0;
```
`run()` 方法会执行指定任务的 `num_total_tasks` 个实例。由于这一次函数调用会导致多个任务的执行，我们将每次对 `run()` 的调用称为一次 _批量任务启动_。

`tasksys.cpp` 中的起始代码包含了 `TaskSystemSerial::run()` 的一个正确但为串行执行的实现，它展示了任务系统如何使用 `IRunnable` 接口来执行批量任务启动。（`IRunnable` 的定义在 `itasksys.h` 中）注意，在每次调用 `IRunnable::runTask()` 时，任务系统会为任务提供一个当前任务标识符（一个介于 0 和 `num_total_tasks` 之间的整数），以及批量任务启动中的任务总数。任务的实现将使用这些参数来确定该任务应执行的工作。

`run()` 方法的一个重要细节是，它必须相对于调用线程同步执行任务。换句话说，当 `run()` 调用返回时，应用程序可以确保任务系统已经完成了批量任务启动中 **所有任务** 的执行。起始代码中提供的 `run()` 的串行实现会在调用线程上执行所有任务，因此满足这一要求。 
### 运行测试 ###

起始代码包含了一套使用你的任务系统的测试应用程序。有关测试框架测试的描述，请参阅 `tests/README.md`；有关测试的具体定义，请参阅 `tests/tests.h`。要运行测试，请使用 `runtasks` 脚本。例如，要运行名为 `mandelbrot_chunked` 的测试，该测试使用批量任务启动来计算曼德勃罗分形图像，每个任务处理图像的一个连续块，可输入以下命令：
```bash
./runtasks -n 16 mandelbrot_chunked
```

不同的测试具有不同的性能特征 —— 有些测试每个任务的工作量很小，而有些则需要进行大量处理。有些测试每次启动会创建大量任务，而有些则很少。有时一次启动中的所有任务计算成本相近，而在其他情况下，一次批量启动中任务的成本是可变的。我们在 `tests/README.md` 中描述了大部分测试，但我们鼓励你查看 `tests/tests.h` 中的代码，以更详细地了解所有测试的行为。

在实现解决方案时，`simple_test_sync` 这个测试可能有助于调试正确性。这是一个非常小的测试，不应用于性能测量，但它足够小，可以使用打印语句或调试器进行调试。请参阅 `tests/tests.h` 中的 `simpleTest` 函数。

我们鼓励你创建自己的测试。查看 `tests/tests.h` 中的现有测试以获取灵感。我们还提供了一个由 `class YourTask` 和 `yourTest()` 函数组成的测试框架，供你按需构建。对于你创建的测试，请确保将它们添加到 `tests/main.cpp` 中的测试列表和测试名称列表中，并相应地调整 `n_tests` 变量。请注意，虽然你可以使用自己的解决方案运行自定义测试，但无法编译参考解决方案来运行你的测试。

`-n` 命令行选项指定任务系统实现可以使用的最大线程数。在上面的示例中，我们选择 `-n 16` 是因为 AWS 实例中的 CPU 有 16 个执行上下文。可以通过命令行帮助（`-h` 命令行选项）查看所有可用的测试列表。

`-i` 命令行选项指定在性能测量期间运行测试的次数。为了准确测量性能，`./runtasks` 会多次运行测试并记录多次运行中的 _最短_ 运行时间；一般来说，默认值就足够了 —— 更大的值可能会得到更准确的测量结果，但会增加测试运行时间。

此外，我们还提供了用于性能评分的测试框架：
```bash
>>> python3 ../tests/run_test_harness.py
```

该框架有以下命令行参数：
```bash
>>> python3 run_test_harness.py -h
usage: run_test_harness.py [-h] [-n NUM_THREADS]
                           [-t TEST_NAMES [TEST_NAMES ...]] [-a]

Run task system performance tests

optional arguments:
  -h, --help            show this help message and exit
  -n NUM_THREADS, --num_threads NUM_THREADS
                        Max number of threads that the task system can use. (16
                        by default)
  -t TEST_NAMES [TEST_NAMES ...], --test_names TEST_NAMES [TEST_NAMES ...]
                        List of tests to run
  -a, --run_async       Run async tests
```

它会生成一份详细的性能报告，如下所示：
```bash
>>> python3 ../tests/run_test_harness.py -t super_light super_super_light
python3 ../tests/run_test_harness.py -t super_light super_super_light
================================================================================
Running task system grading harness... (2 total tests)
  - Detected CPU with 16 execution contexts
  - Task system configured to use at most 16 threads
================================================================================
================================================================================
Executing test: super_super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                9.053     9.022       1.00  (OK)
[Parallel + Always Spawn]               8.982     33.953      0.26  (OK)
[Parallel + Thread Pool + Spin]         8.942     12.095      0.74  (OK)
[Parallel + Thread Pool + Sleep]        8.97      8.849       1.01  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                68.525    68.03       1.01  (OK)
[Parallel + Always Spawn]               68.178    40.677      1.68  (NOT OK)
[Parallel + Thread Pool + Spin]         67.676    25.244      2.68  (NOT OK)
[Parallel + Thread Pool + Sleep]        68.464    20.588      3.33  (NOT OK)
================================================================================
Overall performance results
[Serial]                                : All passed Perf
[Parallel + Always Spawn]               : Perf did not pass all tests
[Parallel + Thread Pool + Spin]         : Perf did not pass all tests
[Parallel + Thread Pool + Sleep]        : Perf did not pass all tests
```

在上述输出中，`PERF` 是你的实现的运行时间与参考解决方案的运行时间之比。因此，小于 1 的值表示你的任务系统实现比参考实现更快。

> [!TIP]
> 苹果电脑用户：虽然我们为 A 部分和 B 部分都提供了参考解决方案的二进制文件，但我们将使用 Linux 二进制文件来测试你的代码。因此，我们建议你在提交之前在 AWS 实例中检查你的实现。如果你使用的是配备 M1 芯片的较新苹果电脑，在本地测试时使用 `runtasks_ref_osx_arm` 二进制文件；否则，使用 `runtasks_ref_osx_x86` 二进制文件。

> [!IMPORTANT]
我们将在 AWS 上使用 `runtasks_ref_linux_arm` 版本的参考解决方案对你的解决方案进行评分。请确保你的解决方案在 AWS ARM 实例上能正常工作。 
### 你需要做什么 ###
你的任务是实现一个能高效利用多核 CPU 的任务执行引擎。我们将根据你实现的正确性（必须正确运行所有任务）和性能对你进行评分。这应该是一个有趣的编码挑战，但也是一项艰巨的工作。为了帮助你保持正确的方向，完成作业的 A 部分，我们要求你实现多个版本的任务系统，逐步增加实现的复杂性和性能。你需要在 `tasksys.cpp/.h` 中定义的类里完成三个实现：
- `TaskSystemParallelSpawn`
- `TaskSystemParallelThreadPoolSpinning`
- `TaskSystemParallelThreadPoolSleeping`

**在 `part_a/` 子目录中实现你在 A 部分的代码，以便与正确的参考实现（`part_a/runtasks_ref_*`）进行比较。**

**专业提示：注意下面的说明是如何采用 “先尝试最简单的改进” 这种方法的。每一步都会增加任务执行系统实现的复杂性，但在每一步中，你都应该有一个能正常工作（完全正确）的任务运行时系统。**

我们还希望你至少创建一个测试，该测试可以测试正确性或性能。更多信息请参阅上面的 “运行测试” 部分。

#### 步骤 1：转向并行任务系统####
**在这一步中，请实现 `TaskSystemParallelSpawn` 类。**

起始代码在 `TaskSystemSerial` 中提供了任务系统的一个可工作的串行实现。在本次作业的这一步中，你将扩展起始代码，以并行方式执行批量任务启动。
- 你需要创建额外的控制线程来执行批量任务启动的工作。注意，`TaskSystem` 的构造函数有一个参数 `num_threads`，它是你的实现可以用来运行任务的 **最大工作线程数**。
- 本着 “先做最简单的事” 的精神，我们建议你在 `run()` 开始时生成工作线程，并在 `run()` 返回之前从主线程中加入这些线程。这将是一个正确的实现，但频繁创建线程会带来显著的开销。
- 你将如何将任务分配给工作线程？你应该考虑静态还是动态地将任务分配给线程？
- 是否存在共享变量（任务执行系统的内部状态），你需要保护它们免受多个线程的同时访问？你可能希望查看我们的 [C++ 同步教程](tutorial/README.md)，以获取有关 C++ 标准库中同步原语的更多信息。

#### 步骤 2：使用线程池避免频繁创建线程####
**在这一步中，请实现 `TaskSystemParallelThreadPoolSpinning` 类。**

你在步骤 1 中的实现会因为每次调用 `run()` 时创建线程而产生开销。当任务计算成本较低时，这种开销尤为明显。此时，我们建议你采用 “线程池” 实现，即任务执行系统预先创建所有工作线程（例如，在 `TaskSystem` 构造期间或首次调用 `run()` 时）。
- 作为初始实现，我们建议你将工作线程设计为持续循环，始终检查是否有更多工作要执行。（线程进入 while 循环直到某个条件为真，通常称为 “自旋”。）工作线程如何确定有工作要做呢？
- 现在要确保 `run()` 实现所需的同步行为并非易事。你需要如何更改 `run()` 的实现，以确定批量任务启动中的所有任务都已完成？

#### 步骤 3：在无事可做时让线程进入睡眠状态####
**在这一步中，请实现 `TaskSystemParallelThreadPoolSleeping` 类。**

步骤 2 实现的一个缺点是，线程在 “自旋” 等待任务时会占用 CPU 核心的执行资源。例如，工作线程可能会循环等待新任务到达。再例如，主线程可能会循环等待工作线程完成所有任务，以便从 `run()` 调用中返回。这会损害性能，因为即使这些线程没有做有用的工作，CPU 资源仍被用于运行它们。

在本次作业的这一部分，我们希望你通过让线程进入睡眠状态，直到它们等待的条件满足，来提高任务系统的效率。
- 你的实现可以选择使用条件变量来实现这种行为。条件变量是一种同步原语，它使线程在等待某个条件存在时进入睡眠状态（不占用 CPU 处理资源）。其他线程 “发出信号”，唤醒等待的线程，查看它们等待的条件是否已满足。例如，如果没有工作要做，你的工作线程可以进入睡眠状态（这样它们就不会占用试图做有用工作的线程的 CPU 资源）。再例如，调用 `run()` 的主应用线程可能希望在等待工作线程完成批量任务启动中的所有任务时进入睡眠状态。（否则，自旋的主线程会占用工作线程的 CPU 资源！）有关 C++ 中条件变量的更多信息，请参阅我们的 [C++ 同步教程](tutorial/README.md)。
- 你在这部分作业中的实现可能会存在棘手的竞态条件，需要仔细考虑。你需要考虑线程行为的许多可能的交错情况。
- 你可能需要考虑编写额外的测试用例来测试你的系统。**作业起始代码包含评分脚本用于评估你代码性能的工作负载，但我们也会使用一组更广泛的工作负载来测试你实现的正确性，这些工作负载并未在起始代码中提供！**

## 第二部分：支持任务图的执行
在作业的 B 部分，你将扩展在 A 部分的任务系统实现，以支持异步启动可能依赖于先前任务的任务。这些任务间的依赖关系会产生任务执行库必须遵守的调度约束。

`ITaskSystem` 接口有一个额外的方法：
```cpp
virtual TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps) = 0;
```
`runAsyncWithDeps()` 与 `run()` 类似，因为它也用于批量启动 `num_total_tasks` 个任务。然而，它在很多方面与 `run()` 不同…… 
### 异步任务启动
首先，使用 `runAsyncWithDeps()` 创建的任务由任务系统与调用线程 **异步** 执行。这意味着 `runAsyncWithDeps()` 应该 **立即** 返回给调用者，即使任务尚未完成执行。该方法返回一个与此次批量任务启动相关联的唯一标识符。

调用线程可以通过调用 `sync()` 来确定批量任务启动何时真正完成。
```cpp
virtual void sync() = 0;
```
`sync()` **仅在与所有先前批量任务启动相关的任务都完成后** 才会返回给调用者。例如，考虑以下代码：
```cpp
// 假设 taskA 和 taskB 是有效的 IRunnable 实例...
std::vector<TaskID> noDeps;  // 空向量
ITaskSystem *t = new TaskSystem(num_threads);

// 批量启动4个任务
TaskID launchA = t->runAsyncWithDeps(taskA, 4, noDeps);

// 批量启动8个任务
TaskID launchB = t->runAsyncWithDeps(taskB, 8, noDeps);

// 此时，与 launchA 和 launchB 相关的任务可能仍在运行
t->sync();

// 此时，与 launchA 和 launchB 相关的所有12个任务都保证已终止
```
如上面注释所述，在调用线程调用 `sync()` 之前，无法保证先前对 `runAsyncWithDeps()` 的调用所产生的任务已经完成。确切地说，`runAsyncWithDeps()` 告诉任务系统执行一次新的批量任务启动，但你的实现可以灵活地在下次调用 `sync()` 之前的任何时间执行这些任务。请注意，此规范意味着无法保证你的实现会在启动 `launchB` 的任务之前先执行 `launchA` 的任务！

### 支持显式依赖关系
`runAsyncWithDeps()` 的第二个关键细节是它的第三个参数：一个 `TaskID` 标识符向量，这些标识符必须指向先前使用 `runAsyncWithDeps()` 进行的批量任务启动。这个向量指定了当前批量任务启动中的任务所依赖的先前任务。**因此，在依赖向量中指定的所有任务完成之前，你的任务运行时不能开始执行当前批量任务启动中的任何任务！** 例如，考虑以下示例：
```cpp
std::vector<TaskID> noDeps;  // 空向量
std::vector<TaskID> depOnA;
std::vector<TaskID> depOnBC;

ITaskSystem *t = new TaskSystem(num_threads);

TaskID launchA = t->runAsyncWithDeps(taskA, 128, noDeps);
depOnA.push_back(launchA);

TaskID launchB = t->runAsyncWithDeps(taskB, 2, depOnA);
TaskID launchC = t->runAsyncWithDeps(taskC, 6, depOnA);
depOnBC.push_back(launchB);
depOnBC.push_back(launchC);

TaskID launchD = t->runAsyncWithDeps(taskD, 32, depOnBC);
t->sync();
```
上面的代码包含四次批量任务启动（任务A：128个任务，任务B：2个任务，任务C：6个任务，任务D：32个任务）。注意，任务B和任务C的启动依赖于任务A。任务D（`launchD`）的批量启动依赖于 `launchB` 和 `launchC` 的结果。因此，虽然你的任务运行时可以按任意顺序（包括并行）处理与 `launchB` 和 `launchC` 相关的任务，但这些启动中的所有任务必须在 `launchA` 的任务完成之后才能开始执行，并且必须在运行时开始执行 `launchD` 的任何任务之前完成。

我们可以将这些依赖关系可视化为一个 **任务图**。任务图是一个有向无环图（DAG），图中的节点对应于批量任务启动，从节点X到节点Y的边表示Y对X的输出的依赖关系。上面代码的任务图如下：
<p align="center">
    <img src="figs/task_graph.png" width=400>
</p>

注意，如果你在具有八个执行上下文的Myth机器上运行上述示例，并行调度 `launchB` 和 `launchC` 的任务可能非常有用，因为单独的任何一个批量任务启动都不足以充分利用机器的所有执行资源。

### 测试
所有后缀为 `Async` 的测试都应用于测试B部分。评分工具中包含的测试子集在 `tests/README.md` 中有描述，所有测试都可以在 `tests/tests.h` 中找到，并在 `tests/main.cpp` 中列出。为了调试正确性，我们提供了一个小测试 `simple_test_async`。查看 `tests/tests.h` 中的 `simpleTest` 函数。`simple_test_async` 足够小，可以使用打印语句或在 `simpleTest` 内部设置断点进行调试。

我们鼓励你创建自己的测试。查看 `tests/tests.h` 中的现有测试以获取灵感。我们还提供了一个由 `class YourTask` 和函数 `yourTest()` 组成的测试框架，供你按需构建。对于你创建的测试，请确保将它们添加到 `tests/main.cpp` 中的测试列表和测试名称列表中，并相应地调整 `n_tests` 变量。请注意，虽然你可以使用自己的解决方案运行自定义测试，但无法编译参考解决方案来运行你的测试。 
### 你需要做什么 ###

你必须扩展在 A 部分中使用线程池（且线程会进入睡眠状态）的任务系统实现，以正确实现 `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()` 和 `TaskSystemParallelThreadPoolSleeping::sync()` 方法。我们同样期望你至少创建一个测试，该测试可以测试正确性或者性能。更多信息请参考上述的“测试”部分。需要说明的是，你需要在报告中描述你自己创建的测试，但我们的自动评分器不会测试你的测试用例。
**在 B 部分，你无需实现其他的 `TaskSystem` 类。**

和 A 部分一样，我们为你提供以下一些开始时的提示：
- 把 `runAsyncWithDeps()` 的行为想象成将一条对应批量任务启动的记录，或者也许是将对应批量任务启动中每个任务的记录推送到一个“工作队列”中，这可能会有所帮助。一旦要处理的工作记录进入队列，`runAsyncWithDeps()` 就可以返回给调用者。
- 本次作业这部分的关键在于进行恰当的记录工作，以跟踪依赖关系。当一个批量任务启动中的所有任务都完成时必须做些什么呢？（此时新的任务可能就可以运行了。）
- 在你的实现中拥有两个数据结构可能会很有帮助：（1）一个表示通过调用 `runAsyncWithDeps()` 添加到系统中，但由于依赖仍在运行的任务而尚未准备好执行的任务的结构（这些任务在“等待”其他任务完成）；（2）一个“就绪队列”，其中的任务不依赖于任何先前的任务完成，并且一旦有工作线程可用于处理它们，就可以安全地运行。
- 在生成唯一的任务启动 ID 时，你无需担心整数溢出问题。我们不会让你的任务系统处理超过 $2^{31}$ 次的批量任务启动。
- 你可以假设所有程序要么只调用 `run()`，要么只调用 `runAsyncWithDeps()`；也就是说，你无需处理 `run()` 调用需要等待所有之前的 `runAsyncWithDeps()` 调用完成的情况。请注意，这个假设意味着你可以通过对 `runAsyncWithDeps()` 和 `sync()` 进行适当的调用来实现 `run()`。
- 你可以假设正在进行的唯一多线程操作是由你的实现创建/使用的多个线程。也就是说，我们不会生成额外的线程并从那些线程中调用你的实现。

**在 `part_b/` 子目录中实现你在 B 部分的代码，以便与正确的参考实现（`part_b/runtasks_ref_*`）进行比较。**

## 评分 ##
本次作业的分数分配如下：
**A 部分（50 分）**
- `TaskSystemParallelSpawn::run()` 的正确性得 5 分 + 其性能得 5 分。（共 10 分）
- `TaskSystemParallelThreadPoolSpinning::run()` 和 `TaskSystemParallelThreadPoolSleeping::run()` 的正确性各得 10 分 + 这两个方法的性能各得 10 分。（共 40 分）

**B 部分（40 分）**
- `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`、`TaskSystemParallelThreadPoolSleeping::run()` 和 `TaskSystemParallelThreadPoolSleeping::sync()` 的正确性得 30 分。
- `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`、`TaskSystemParallelThreadPoolSleeping::run()` 和 `TaskSystemParallelThreadPoolSleeping::sync()` 的性能得 10 分。对于 B 部分，你可以忽略 `Parallel + Always Spawn` 和 `Parallel + Thread Pool + Spin` 的结果。也就是说，你只需使每个测试用例通过 `Parallel + Thread Pool + Sleep` 的测试。

**报告（10 分）**
- 请参考“提交”部分以获取更多详细信息。

对于每个测试，实现结果与提供的参考实现相差在 20% 以内的将获得完整的性能分数。性能分数仅授予返回正确答案的实现。如前所述，我们也可能会使用一组更广泛的工作负载来测试你的实现的 _正确性_，这些工作负载并未在起始代码中提供。

## 提交 ##
请使用 [Gradescope](https://www.gradescope.com/) 提交你的作业。你的提交内容应包括你的任务系统代码，以及一份描述你实现的报告。我们期望在提交的内容中包含以下五个文件：
* part_a/tasksys.cpp
* part_a/tasksys.h
* part_b/tasksys.cpp
* part_b/tasksys.h
* 你的报告 PDF 文件（提交到 Gradescope 的报告作业中）

#### 代码提交 ####
我们要求你将源文件 `part_a/tasksys.cpp|.h` 和 `part_b/tasksys.cpp|.h` 放在一个压缩文件中提交。你可以创建一个目录（例如命名为 `asst2_submission`），其中包含 `part_a` 和 `part_b` 子目录，将相关文件放入其中，通过运行 `tar -czvf asst2.tar.gz asst2_submission` 压缩该目录，然后上传。请将 **压缩文件** `asst2.tar.gz` 提交到 Gradescope 上的作业 *作业 2（代码）* 中。

在提交源文件之前，请确保所有代码都是可编译且可运行的！我们应该能够将这些文件放入一个干净的起始代码树中，输入 `make`，然后无需手动干预即可执行你的程序。

我们的评分脚本将运行起始代码中提供给你的检查代码，以确定性能分数。 _我们还将在起始代码中未提供的其他应用程序上运行你的代码，以进一步测试其正确性！_ 评分脚本将在作业截止日期之后运行。

#### 报告提交 ####
请在 Gradescope 上的作业 *作业 2（报告）* 中提交一份简短的报告，内容需涵盖以下方面：
1. 描述你的任务系统实现（1 页纸的篇幅即可）。除了对其工作原理的一般性描述外，请务必回答以下问题：
    - 你是如何决定管理线程的？（例如，你是否实现了线程池？）
    - 你的系统如何将任务分配给工作线程？你使用的是静态分配还是动态分配？
    - 在 B 部分中，你是如何跟踪依赖关系以确保任务图的正确执行的？
2. 在 A 部分中，你可能已经注意到，更简单的任务系统实现（例如，完全串行的实现，或者每次启动都生成线程的实现）的性能与更高级的实现一样好，有时甚至更好。请解释为什么会出现这种情况，并以某些测试为例进行说明。例如，在什么情况下顺序任务系统实现的性能最佳？为什么？在什么情况下每次启动都生成线程的实现与使用线程池的更高级并行实现的性能一样好？又在什么情况下不一样呢？
3. 描述你为本次作业实现的一个测试。该测试做了什么，它旨在检查什么，以及你是如何验证你对作业的解决方案在该测试中表现良好的？你添加的测试结果是否导致你更改了作业的实现？ 