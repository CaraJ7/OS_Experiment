## Git 常用指令

1. 推送到指定的远程分支

   `git push remote_name local_branch:remote_branch`

2. 从远程分支上获取

   `git pull = git fetch origin util + git merge FETCH_HEAD`

   一般直接使用`git pull`即可

   `git pull <远程主机名> <远程分支名>:<本地分支名>`

3. 从远程分支上获取本地没有的分支

   `git checkout -b 本地分支名 远程仓库名/远程分支名`

4. 设置分支追踪某个远程分支

   `git branch --set-upstream-to=<远程主机名>/<远程分支名> <本地分支名>`

5. 查看远程分支的提交信息
   `git log 远程仓库名/远程分支名`