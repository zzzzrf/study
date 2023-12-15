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
rdesktop -u USERNAME -p PASSWD IP -g 1920x1080 -f

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
