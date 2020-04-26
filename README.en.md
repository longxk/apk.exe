Apk.exe can generate a single ELF executable for Android from your Java/Kotlin code.

Tested on Android 8/9/10, support armeabi-v7a, arm64-v8a, x86, x86_64 ABIs.

## How To Use

1. Create Android Application project

2. Add Gradle plugin dependency (jcenter)

```gradle
buildscript {
    dependencies {
        classpath 'com.longxk.apk_exe:plugin:0.0.1'
    }
}

plugins {
    id 'com.longxk.apk_exe'
}
```

3. Add an entry class with full name: **Main**, and make sure there is a static **main** method in it, just like regular Java application.

4. Run bundleApkExeRelease

5. Generated exectuables will be store in directory **build/outputs/apkexe/Release**.

[Demo Application](https://github.com/longxk/apk.exe/tree/master/demo)

## Under The Hood
Apk.exe essentially merges a loader executable file and dex files created by Androind Studio into a single executable file,
the loader will guide Android's app_process program to load loader executable file itself, and modify behaviours of app_process,
makes app_process load and run the loader as a real dex file.

## License
This project is released under the [WTFPL LICENSE](http://www.wtfpl.net/ "WTFPL LICENSE").