Apk.exe将Java/Kotlin代码转换为可以在Android系统上运行的单一可执行文件。

## 使用方法
1. 新建一个Application类型的Android项目

2. 新增gradle plugin，暂不可用，上传中

```gradle
plugins {
    id 'com.longxk.apk_exe' version '0.0.1'
}
```

3. 新增入口类Main，并确保入口类中含有一个静态的main方法，与Java程序的入口方法一致

4. 运行bundleApkExeRelease Task

5. 生成的可执行文件保存在build/outputs/apkexe/Release目录下

具体可以参考demo

## 原理
Apk.exe本质上是一个Android系统app_process引导器，用于引导的可执行文件，和包含dex的apk文件，被合并在同一个文件中。

也就是说Apk.exe既是一个标准的ELF文件，也是一个标准的apk(zip)文件，Apk.exe启动后引导app_process将自身当成apk文件运行。

## 背景
由于经常需要写一些在Android系统上运行的命令行小工具，但是对C/C++又不太熟悉，所以就尝试用Java来开发。

比较直接的做法是：

Test.java：
```java
public class Test {
    public static void main(String[] args) {
        System.out.println("Hello");
    }
}
```

编译运行：
```bash
javac Test.java
d8 Test.class
adb push classes.dex /data/local/tmp
adb shell app_process -Djava.class.path=/data/local/tmp/classes.dex . Test
```

但步骤比较繁琐，生成的dex文件也不是真正的可执行文件，使用上有诸多不便。

app_process同样支持zip文件作为输入，zip文件的格式比较特别，它的meta信息放置在文件末尾，
这使得很多zip文件解析程序支持跳过拼接在zip文件开头的额外无关内容。

可以试一下如下命令，拼接出来的test2.zip文件同样可以被正确解析和解压。

```bash
zip test.zip classes.dex
echo xxxxxxxxx | cat - test.zip > test2.zip
unzip -l test2.zip
```

如果Android系统的zip解析库也支持同样的特性，那就可以制作一个针对app_process的引导程序，并将引导程序和包含dex的zip文件拼接在一起。

由于可执行文件末尾拼接的额外内容，并不影响程序正确执行，运行时引导程序引导app_process将自身作为zip文件加载，这样就使得dex文件可以作为一个真正的可执行文件运行。

不过不幸地是这一方案并不奏效。

检查app_process对apk文件的解析流程，可以发现该方案在最开始对文件魔法数字的检查那一步便失败了
（位于/art/libdexfile/dex/art_dex_file_loader.cc:Open:OpenAndReadMagic, OpenWithMagic）,
因为我们拼接获得的文件前半部分是ELF可执行文件，魔法数必然与Zip文件的魔法数不同。

如何能绕过这一检查呢？直接修改魔法数肯定是不可取的，那样就变成不可执行文件了。
比较合适的方法是采用Hook相关函数调用的方式来修改这一行为。

Linux上有个简单又好用的技巧：LD_PRELOAD环境变量。这个环境变量指定的动态库会先于其他任何动态库加载，
也就是说运行时这个动态库中的符号会先于其他的被查找，因此得以有机会覆写其他同名符号。

简单实验一下，尝试Hook对open函数的调用，并输出一行日志：

```c
#include <dlfcn.h>
#include <stdio.h>

int (*open_orig)(const char *, int);

int open(const char *pathname, int flags) {
    if (open_orig == NULL) {
        open_orig = (int (*)(const char *, int))dlsym(RTLD_NEXT, "open");
    }

    int fd = open_orig(pathname, flags);
    if (fd >= 0) {
        printf("hook file open: %s\n", pathname);
    }
    return fd;
}
```

编译运行：
```bash
aarch64-linux-android29-clang -shared -fPIC -o libopen.so open.c

adb push libopen.so /data/local/tmp

adb shell LD_PRELOAD=/data/local/tmp/libopen.so cat /dev/null
```

输出：
```    
hook file open: /dev/null
```

可以看出对open函数调用的Hook奏效了。

回到前面的需求场景上，要修改OpenAndReadMagic函数的实现以返回ZIP文件的魔法数，
而OpenAndReadMagic函数的主要构成仅仅是一个read函数调用，也就是说，只需要Hook对read函数的调用，
并将文件的偏移量移到ZIP文件的开始位置，就能达到目的。

当然，绕过文件魔法数的检查只是开始，ZIP文件的解析上还有许多问题需要解决，
但是理论上都可以通过hook相关函数调用，并修改文件偏移来解决，这里不再一一赘述，具体参考hook部分的源码：hookext.c。

到了这一步，引导程序和包含dex的ZIP文件已经合二为一，但是还有一个用于Hook的动态库需要额外加载，使用还是比较繁琐。

Android 5.0及以后系统上的可执行文件都是PIE（地址无关可执行文件），实际上跟动态库没有区别，同样可以作为动态库加载。

```bash
adb shell file /system/bin/sh
/system/bin/sh: ELF shared object, 64-bit LSB arm64, dynamic (/system/bin/linker64)
```
```bash
adb shell file /system/lib64/libz.so
/system/lib64/libz.so: ELF shared object, 64-bit LSB arm64
```

于是可以将传递给app_process的环境变量LD_PRELOAD指向引导程序本身，并在引导程序上导出相关Hook符号，就能实现对app_process行为的修改，同时无需引入额外的动态库。
只需要将前面提到的hook部分源文件与引导程序一起编译，并配合相关编译参数控制符号导出即可，具体参考CMakeLists.txt文件。

## 为什么不用C/C++写
当性能不是问题的时候，Java/Kotlin的开发效率要显著高于C/C++，特别是对C/C++不熟练的开发者。

## 为什么不用Kotlin Native写
Kotlin Native产生的可执行文件非常大，并且无法使用Java和Android的接口。

## 许可
该项目使用[WTFPL](http://www.wtfpl.net/ "WTFPL LICENSE")许可