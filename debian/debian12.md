# debian12

* 修改镜像源

```bash
zrf@debian:~$ cat /etc/apt/sources.list
deb https://mirrors.aliyun.com/debian/ bookworm main non-free contrib
deb-src https://mirrors.aliyun.com/debian/ bookworm main non-free contrib
deb https://mirrors.aliyun.com/debian-security/ bookworm-security main
deb-src https://mirrors.aliyun.com/debian-security/ bookworm-security main
deb https://mirrors.aliyun.com/debian/ bookworm-updates main non-free contrib
deb-src https://mirrors.aliyun.com/debian/ bookworm-updates main non-free contrib
deb https://mirrors.aliyun.com/debian/ bookworm-backports main non-free contrib
deb-src https://mirrors.aliyun.com/debian/ bookworm-backports main non-free contrib

```

* 将目录修改为英文名字

```bash
zrf@debian:~$ xdg-user-dirs-gtk-update

(process:10330): Gtk-WARNING **: 10:12:40.761: Locale not supported by C library.
 Using the fallback 'C' locale.
Moving DESKTOP directory from 桌面 to Desktop
Moving DOWNLOAD directory from 下载 to Downloads
Moving TEMPLATES directory from 模板 to Templates
Moving PUBLICSHARE directory from 公共 to Public
Moving DOCUMENTS directory from 文档 to Documents
Moving MUSIC directory from 音乐 to Music
Moving PICTURES directory from 图片 to Pictures
Moving VIDEOS directory from 视频 to Videos
zrf@debian:~$ export LANG=zh_CN.UTF-8
zrf@debian:~$ xdg-user-dirs-gtk-update
```

* 修改终端的字体

[下载consolas字体](https://www.dafontfree.io/download/consolas/#google_vignette)

```bash
zrf@debian:~$ sudo mkdir -p  /usr/share/fonts/truetype/consolas

zrf@debian:~$ sudo unzip Downloads/Consolas-Font.zip -d /usr/share/fonts/truetype/consolas/
Archive:  Downloads/Consolas-Font.zip
  inflating: /usr/share/fonts/truetype/consolas/CONSOLAB.TTF  
  inflating: /usr/share/fonts/truetype/consolas/consolai.ttf  
  inflating: /usr/share/fonts/truetype/consolas/Consolas.ttf  
  inflating: /usr/share/fonts/truetype/consolas/consolaz.ttf  
  inflating: /usr/share/fonts/truetype/consolas/CONSOLA.TTF 

zrf@debian:~$ sudo fc-cache -f -v

zrf@debian:~$ fc-list |grep consolas
/usr/share/fonts/truetype/consolas/CONSOLAB.TTF: Consolas:style=Bold
/usr/share/fonts/truetype/consolas/consolaz.ttf: Consolas:style=Bold Italic
/usr/share/fonts/truetype/consolas/consolai.ttf: Consolas:style=Italic
/usr/share/fonts/truetype/consolas/Consolas.ttf: Consolas:style=Italic
/usr/share/fonts/truetype/consolas/CONSOLA.TTF: Consolas:style=Regular

zrf@debian:~$ sudo vim /etc/fonts/conf.d/60-latin.conf 
 <alias>
  <family>monospace</family>
  <prefer>
   <family>Consolas</family>
   <family>Noto Sans Mono</family>
   <family>DejaVu Sans Mono</family>
   <family>Inconsolata</family>
   <family>Andale Mono</family>
   <family>Courier New</family>
   <family>Cumberland AMT</family>
   <family>Luxi Mono</family>
   <family>Nimbus Mono L</family>
   <family>Nimbus Mono</family>
   <family>Nimbus Mono PS</family>
   <family>Courier</family>
  </prefer>
 </alias>

```

* 打开终端快捷键
gnome-terminal

* rdesktop

```bash
rdesktop -u USERNAME -p PASSWD IP -g 1920x1080 -f
```

* xfreerdp

```bash
xfreerdp /u:USERNAME /p:PASSWD /v:IP /size:1920x1080 /f /smart-sizing
```

* clash

```bash
zrf@debian:~/.config/clash$ ls
cache.db  config.yaml  Country.mmdb
zrf@debian:~/.config/clash$ whereis clash 
clash: /usr/local/bin/clash

zrf@debian:~$ cat /etc/systemd/system/clash.service 
[Unit]
Description= proxy
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/clash -f /home/zrf/.config/clash/config.yaml

[Install]
WantedBy=multi-user.target

zrf@debian:~$ sudo systemctl daemon-reload

zrf@debian:~$ systemctl start clash.service 
zrf@debian:~$ ps -aux | grep clash
root       25803  6.2  0.1 1244272 23452 ?       Ssl  11:38   0:01 /usr/local/bin/clash -f /home/zrf/.config/clash/config.yaml
zrf        25858  0.0  0.0   6560  2224 pts/1    S+   11:39   0:00 grep clash

zrf@debian:~$ systemctl status clash.service 
● clash.service - proxy
     Loaded: loaded (/etc/systemd/system/clash.service; disabled; preset: enabled)
     Active: active (running) since Fri 2023-12-15 11:38:53 CST; 52s ago
   Main PID: 25803 (clash)
      Tasks: 14 (limit: 19007)
     Memory: 21.5M
        CPU: 1.847s
     CGroup: /system.slice/clash.service
             └─25803 /usr/local/bin/clash -f /home/zrf/.config/clash/config.yaml
zrf@debian:~$ systemctl enable clash.service 
Created symlink /etc/systemd/system/multi-user.target.wants/clash.service → /etc/systemd/system/clash.service.
zrf@debian:~$ 
zrf@debian:~$ systemctl status clash.service 
● clash.service - proxy
     Loaded: loaded (/etc/systemd/system/clash.service; enabled; preset: enabled)
     Active: active (running) since Fri 2023-12-15 11:38:53 CST; 1min 4s ago
   Main PID: 25803 (clash)
      Tasks: 14 (limit: 19007)
     Memory: 21.5M
        CPU: 1.850s
     CGroup: /system.slice/clash.service
             └─25803 /usr/local/bin/clash -f /home/zrf/.config/clash/config.yaml
zrf@debian:~$ 
```

* git

无法自动补全

```bash
curl https://raw.githubusercontent.com/git/git/master/contrib/completion/git-completion.bash -o ~/.git-completion.bash

# 追加配置
zrf@debian:~$ tail .bashrc 

if [ -f ~/.git-completion.bash ]; then 
. ~/.git-completion.bash 
fi

# 重新加载
zrf@debian:~$ source ~/.bashrc
```

重命名：

```bash
git config --global alias.st status
```

代理：

```bash
git config --global http.proxy http://proxy.example.com:port
git config --global https.proxy https://proxy.example.com:port

git config --global --unset http.proxy
git config --global --unset https.proxy
```

忽略文件：

```bash
# 如果你已经将这个文件提交到了 Git 仓库，并且想要忽略后续对它的修改，你需要使用以下命令：
git update-index --skip-worktree <file>

# 恢复对文件的跟踪
git update-index --no-skip-worktree <file>
```

* samba

```bash
tail /etc/samba/smb.conf 
[USR]
    path = /home/USR
    browseable = no
    writable = yes
    valid user = USR

zrf@debian:~$ sudo smbpasswd -a USR

```

git commit

```text
feat（功能）:
用于表示添加了新的功能或特性。

fix（修复）:
用于表示修复了某个bug。

chore（日常任务）:
用于表示完成了日常的维护任务，如更新依赖库、改进构建过程等。

docs（文档）:
用于表示更新了项目的文档。

style（样式）:
用于表示代码样式的变更，如格式化代码、修正缩进等，但没有影响代码逻辑。

refactor（重构）:
用于表示对代码进行了重构，优化了代码结构或清理了代码，但没有添加新功能或修复 bug。

test（测试）:
用于表示添加或更新了测试用例。

perf（性能）:
用于表示优化了代码的性能。

build（构建）:
用于表示更改了构建系统或外部依赖项。

ci（持续集成）:
用于表示更新了持续集成流程和脚本。

revert（回退）:
用于表示回退了之前的提交。
```

* strongswan

必备工具

```bash
# 开启sftp
zrf@debian:~/git/strongswan$ cat testing/hosts/default/etc/ssh/sshd_config | grep sftp
Subsystem sftp /usr/lib/openssh/sftp-server

zrf@debian:~/git/strongswan/testing$ sudo apt install debootstrap libvirt-clients libvirt-daemon-system

zrf@debian:~/git/strongswan/testing$ sudo adduser libvirt-qemu
zrf@debian:~/git/strongswan/testing$ sudo adduser libvirt-qemu kvm

zrf@debian:~/git/strongswan/testing$ git diff testing.conf
-: ${BASEIMGMIRROR=http://http.debian.net/debian}
+: ${BASEIMGMIRROR=http://mirrors.aliyun.com/debian}
```

* linux kernel

```bash
./scripts/clang-tools/gen_compile_commands.py


CompileFlags: 
  Add:  -ferror-limit=0
  Remove: [
            -mpreferred-stack-boundary=*,
            -mindirect,
            -mindirect-branch-register,
            -mindirect-branch=thunk-extern,
            -mindirect-branch-cs-prefix,
            -fno-allow-store-data-races,
            -fconserve-stack,
            -ftrivial-auto-var-init=*,
          ]
Diagnostics:
  ClangTidy:
    Remove: [
              bugprone-sizeof-expression,
            ]
```

* docker

安装

```bash
# 卸载旧版本
$ for pkg in docker.io docker-doc docker-compose podman-docker containerd runc; do sudo apt-get remove $pkg; done

# 配置apt源
# Add Docker's official GPG key:
sudo apt-get update
sudo apt-get install ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/debian/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Add the repository to Apt sources:
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update

# 安装
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# 测试
sudo docker run hello-world

# 加入用户组
zrf@debian:~$ cat  /etc/group | grep docker
docker:x:993:

zrf@debian:~$ sudo usermod -aG docker zrf
zrf@debian:~$ cat  /etc/group | grep docker
docker:x:993:zrf

# 修改sock组权限
zrf@debian:~$ ls -al /var/run/docker.sock
srw-rw-rw- 1 root docker 0 12月15日 16:57 /var/run/docker.sock
```

* vim

```bash
curl -fLo ~/.vim/autoload/plug.vim --create-dirs https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim


zrf@debian:~/.vim$ cat vimrc 

call plug#begin()
Plug 'junegunn/vim-easy-align'
Plug 'vim-airline/vim-airline'
call plug#end()
