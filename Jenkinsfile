pipeline {
    agent none
    options {
        timeout(time: 30, unit: 'MINUTES') 
    }
    stages {
        stage("Build and Test") {
            parallel {
                stage("Windows") {
                   agent { label 'mobile-dotnet-windows2022-01' }
                   environment {
                       BRANCH = "${BRANCH_NAME}"
                   }
                   steps {
                       powershell 'jenkins\\jenkins_win.ps1'
                   }
                }
                stage("Apple") {
                    agent { label 'mobile-mac-mini'  }
                    environment {
                        BRANCH = "${BRANCH_NAME}"
                        GH_PAT = credentials("cbl-bot-github-pat")
                        KEYCHAIN_PWD = credentials("mobile-mac-mini-keychain")
                    }
                    steps {
                        sh 'jenkins/jenkins_ios.sh'
                        sh 'scripts/coverage_macos.sh --push'
                    }
                }
                stage("Linux") {
                    agent { label 's61113u16 (litecore)' }
                    environment {
                       BRANCH = "${BRANCH_NAME}"
                    }
                    steps {
                        sh 'jenkins/jenkins_unix.sh'
                    }
                }
            }
        }
    }
}
