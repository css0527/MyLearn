# error:404 Not Found

系统软件源（仓库）中的包列表过期了，导致 apt 去下载 libwebkit2gtk-4.1-0 时，服务器返回 404 Not Found（因为该版本的文件已被更新的版本替换）。

1.更新软件包列表
sudo apt update

2.修复依赖关系
sudo apt -f install

3.验证安装
sudo dpkg -i ./Clash.Verge__amd64.deb
如果还是提示依赖问题，可以再执行一次 sudo apt -f install 收尾。
