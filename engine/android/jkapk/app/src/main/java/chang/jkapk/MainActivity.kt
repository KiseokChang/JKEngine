// #114 jkapk (T1) — hello 1-line TextView.
package chang.jkapk

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val tv = TextView(this)
        tv.text = "hello"
        tv.gravity = Gravity.CENTER
        tv.setPadding(48, 48, 48, 48)
        setContentView(tv)
    }
}