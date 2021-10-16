## A Guidance to Lab5

### 1.Eliminate allocation from sbrk()

我找了半天到底为什么会调用`sys_sbrk()`，原来是`sh.c`里面的`parse_cmd`里面用到了`malloc`函数，而`malloc`函数就是主要的在用户态调用`sbrk`的函数。调用了这个给子进程（相对于`sh`），子进程在运行的时候访问了没有分配的虚拟内存，导致出现`usertrap`进入内核，内核直接`exit`了子进程，再由于`exit`函数的注释，父进程在`wait`的时候将子进程杀掉，但是杀掉的时候有一步`freeproc`会调用`uvmunmap`，这个函数是根据进程的`sz`来的，由于`sz`中有没有被实际分配物理地址的，所以会产生`uvmunmap`错误。



### 3.Lazytests and Usertests

这里我们认为，如果真的传进来了地址，这个地址只能是从用户态拷贝到内核或者从内核拷贝到用户态，也即`copyin,copyinstr,copyout`三个函数,并且认为只要需要用到用户态的虚拟地址，一定是从这三个函数用，不会自己私自去检查

有两个问题没解决，

一个是`usertests/sbrkarg`，在`lazy_isunmap`里面对`if`的第三个的设置，我不理解为什么要判断页表项是否有效，正常来说只要这个页表项不对应物理地址，就应该已经是0了。

第二个是`lazytests/outofmemory`，在测试中进入这个函数的时候，进程的`sz`已经远远大于`MAXVA`了，所以我不得不加了第一个`if`让他`break`掉。**md老子服了，sys_sbrk的时候用的是一个int来接收一个uint64`sz`，那nm能不超`MAXVA`吗，高位直接`ffff`了好的吧，改一下`sys_sbrk`，用uint64去接`sz`，这样就不需要前面的那个if了**，但是时间也要稍微长一点，因为这个是真的没空间了才返回的 