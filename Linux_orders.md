### Linux常用命令记录

1. `~` 表示的是当前用户的个人目录地址，比如/home/jdz

   提示符$表示普通用户，#表示root用户

2. tab 补全命令

   ctrl+L 清屏

3. `ls` 列出目录 `-l`显示详细信息

4. 文件夹操作

   + `mkdir` 创建文件夹
   + `rmdir` 删除文件夹

5. 路径名表示

   `~` 表示当前用户的home目录

   `.` 表示目前所在的目录，比如 ./hello 执行当前路径下的hello文件，若是要表示当前目录下的文件，是需要加`/`的

   `..` 表示上一级的目录

   `\xxx`表示根目录，就是最上面的那个目录

6. 复制 `cp`

   `cp source destination`，例如`cp ./hello ..`把当前目录下的hello拷贝到上一级目录上

7. 移动文件`mv`，和上同理 

8. 查找文件 `find`

   `find . -name "*.c"`

9. 查找文件中的内容`grep`

   `grep test test*`

   查找前缀有“test”的文件包含“test”字符串的文件 

10. 查看某个命令的帮助 `man`

    `man -f [args]`或者`-a`	,按q退出

11. 任务管理器

    `top` 实时查看状态

    `ps`	process status，一般`ps -f`查看当前，`ps -aux`查看全部，包括其他user的

12. 打包和解包`tar`

    首先要弄清两个概念：打包和压缩。打包是指将一大堆文件或目录变成一个总的文件；压缩则是将一个大的文件通过一些压缩算法变成一个小文件。

    为什么要区分这两个概念呢？这源于Linux中很多压缩程序只能针对一个文件进行压缩，这样当你想要压缩一大堆文件时，你得先将这一大堆文件先打成一个包（tar命令），然后再用压缩程序进行压缩（gzip bzip2命令）。

    linux下最常用的打包程序就是tar了，使用tar程序打出来的包我们常称为tar包，tar包文件的命令通常都是以.tar结尾的。生成tar包后，就可以用其它的程序来进行压缩。

    ```makefile
    tar -cvf log.tar log2012.log  仅打包成tar
    
    tar -zcvf log.tar.gz log2012.log 打包成tar并用gzip压缩
    tar -zxvf file.tar.gz	对应解压
    
    tar -jcvf log.tar.bz2 log2012.log 打包成tar并用bz2压缩
    tar -jxvf file.tar.bz2	对应解压
    
    zip FileName.zip DirName 用zip压缩
    ```

13. 重定向 

    | command > file  | 将输出重定向到 file             |
    | --------------- | ------------------------------- |
    | command < file  | 将输入重定向到 file             |
    | command >> file | 将输出以追加的方式重定向到 file |

14. 管道

    如果要数一个目录下ls输出了几个文件，一种做法是

    ```
    ls > files.txt
    wc -l files.txt
    ```

    但是用管道就一步

    `ls | wc -l`

    前面写后面读，管道本质上就是一个文件	

15. 传递参数`xargs`

    用于将前一条指令的输出传递给`xargs`后面的指令，当做输入的参数。并且，如果这个输出是用空格/回车隔开的，那么是将每一个输出都当做一次参数的传递，`xargs`每次对其中的**每一个**逐个执行(在linux中，这是在`xargs`命令后面设置了`-n 1`的情况）。例：

    `find . b | xargs grep hello`

    will run "grep hello" on each file named b in the directories below "."

    ```shell
    $ echo "1\n2" | xargs echo line
    line 1
    line 2
    ```

​    

​    