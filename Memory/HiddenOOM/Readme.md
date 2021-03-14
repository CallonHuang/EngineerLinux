# 嵌入式应用上的OOM

在嵌入式Linux的工程应用上，若要列举最令工程师头疼的几个TOP问题，那么OOM必属其一了。由于嵌入式上的内存资源非常有限，所以这类问题出现概率也较高。本章并不讨论内存不足导致的OOM问题（因为这往往与业务强相关），而是讨论一些隐性的OOM问题，这类问题并不像`malloc`未`free`那么显而易见，若是对底层原理缺乏了解，往往还可能在不经意间写出类似代码。

1. Thread Stack Leaked
2. Too Much Socket Buff
3. Too Much Cache
4. Memory Fragmentation

每一个专题将对问题展开进行阐述，希望能让大家掌握相关内存知识，并在开发时关注到更多的内存点。