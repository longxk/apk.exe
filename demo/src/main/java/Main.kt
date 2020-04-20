import android.content.Intent
import android.net.Uri
import android.os.Environment
import android.util.Log
import com.github.kittinunf.fuel.httpGet
import com.github.kittinunf.result.Result;
import java.lang.reflect.Method

private fun printLine() {
    println("-------------------------------------------------------------\n")
}

object Main {
    @JvmStatic
    fun main(args: Array<String>) {
        println("Hello ${ if (args.isNotEmpty()) args[0] else "World" }!")
        printLine()

        val extPath = Environment.getExternalStorageDirectory()
        println("External storage path: ${extPath.absolutePath}")
        extPath.list { dir, name ->
            println("$dir/$name")
            true
        }
        printLine()

        println("Starting play store")
        startActivity()
        printLine()

        println("Requesting")
        val httpAsync = "https://httpbin.org/get"
                .httpGet()
                .responseString { _, _, result ->
                    when (result) {
                        is Result.Failure -> {
                            val ex = result.getException()
                            println(ex)
                        }
                        is Result.Success -> {
                            val data = result.get()
                            println(data)
                        }
                    }
                }

        httpAsync.join()
        printLine()
    }
}

private fun getMethodByName(obj: Any?, name: String?): Method? {
    if (obj == null) return null
    val methods = obj.javaClass.methods
    for (method in methods) {
        if (name == method.name) {
            return method
        }
    }
    return null
}

private fun getActivityManager(): Any? {
    return try {
        val amnClass = Class.forName("android.app.ActivityManagerNative")
        val method = amnClass.getMethod("getDefault")
        method.invoke(null)
    } catch (e: Exception) {
        Log.e("APK.EXE", "invoke getActivityManager error: $e")
        null
    }
}

private fun startActivity() {
    try {
        val am = getActivityManager()
        val method = getMethodByName(am, "startActivity")
        val intent = Intent(Intent.ACTION_VIEW)
        intent.data = Uri.parse("https://play.google.com/store/apps/details?id=com.github.android")
        intent.setPackage("com.android.vending")
        method?.invoke(am, null, "", intent, intent.type, null, null,
                0, Intent.FLAG_ACTIVITY_NEW_TASK, null, null)
    } catch (e: java.lang.Exception) {
        Log.e("APK.EXE", "startActivity error: $e")
    }
}