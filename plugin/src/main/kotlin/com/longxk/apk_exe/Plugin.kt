package com.longxk.apk_exe;

import com.android.build.gradle.AppExtension
import com.android.build.gradle.internal.tasks.factory.dependsOn
import org.gradle.api.Project
import org.gradle.api.file.RelativePath
import org.gradle.api.tasks.*
import org.gradle.api.tasks.bundling.Zip


private const val LOADER = "app_process_loader.so"
private const val OUTPUT_NAME = "app"
private const val OUTPUT_EXT = "apk"

class Plugin : org.gradle.api.Plugin<Project> {

    override fun apply(project: Project) { with(project) {
        if (!hasProperty("android")) {
            throw Exception("not a android project")
        }

        val version = Plugin::class.java.`package`.implementationVersion
        dependencies.add("implementation", "com.longxk.apk_exe:loader:$version")

        val android = extensions.getByName("android") as AppExtension

        afterEvaluate { android.applicationVariants.all { variant ->
            if (variant.outputs.size < 1) {
                return@all
            }

            val inputApkFile = variant.outputs.first().outputFile
            val suffix = variant.flavorName.capitalize() + variant.buildType.name.capitalize()
            val workingDir = "$buildDir/intermediates/apkexe/$suffix"
            val unpackedDir = "$workingDir/unpacked"
            val repackedApkFile = "$workingDir/$OUTPUT_NAME.$OUTPUT_EXT"
            val outputDir = "$buildDir/outputs/apkexe/$suffix"

            val unpackTask = tasks.register("unpackApk$suffix", Copy::class.java) { with(it) {
                from(zipTree(inputApkFile))
                into(unpackedDir)
                include("*.dex")
                include("**/$LOADER")
            }}
            unpackTask.dependsOn(variant.assembleProvider.get())

            val repackApkTask = tasks.register("repackApk$suffix", Zip::class.java) { with(it) {
                archiveFileName.set("$OUTPUT_NAME.$OUTPUT_EXT")
                destinationDirectory.set(file(workingDir))
                from(unpackedDir)
                include("*.dex")
            }}
            repackApkTask.dependsOn(unpackTask)

            /**
             * import repacked apk file as input here,
             * to make sure the task outputs update correctly.
             */
            val bundleApkExeTask = tasks.register("bundleApkExe$suffix", Copy::class.java) { with(it) {
                includeEmptyDirs = false
                from(unpackedDir)
                from(repackedApkFile)
                include("**/*.apk", "**/*.so")
                into(outputDir)
                eachFile { fileCopyDetails ->
                    val abi = fileCopyDetails.relativePath.parent.lastName ?: ""
                    if (abi.isEmpty()) {
                        fileCopyDetails.exclude()
                    } else {
                        val fileName = "$OUTPUT_NAME.$abi.$OUTPUT_EXT"
                        fileCopyDetails.relativePath = RelativePath(true, fileName)
                    }
                }

                doLast {
                    val apkBytes = file(repackedApkFile).readBytes()
                    file(outputDir).listFiles()?.filter { f ->
                        f.name.matches("$OUTPUT_NAME\\..+\\.$OUTPUT_EXT".toRegex())
                    }?.forEach { f ->
                        f.appendBytes(apkBytes)
                    }
                }
            }}

            bundleApkExeTask.dependsOn(repackApkTask)
        }}
    }}
}