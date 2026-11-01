// #126 스파이크-B — docs/101 §5 시험판 (폐기 코드: 라인에 병입하지 않음).
// jkapk(#123 전담 세션 소유) 비접촉 — 완전 별도 프로젝트.
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
    }
}
rootProject.name = "spike-b"
include(":app")