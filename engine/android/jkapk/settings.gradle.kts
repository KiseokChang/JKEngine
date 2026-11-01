pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.FAIL_ON_PROJECT_REPOS
    repositories {
        google()
        mavenCentral()
        // #123 폰 브라우저 T5 — GeckoView 조달기(Mozilla 공식 Maven — 핀은
        // build.gradle.kts: 159.0.20261022150525 = 2026-10-22 안정판, 실측 조회).
        maven { url = uri("https://maven.mozilla.org/maven2") }
    }
}
rootProject.name = "jkapk"
include(":app")