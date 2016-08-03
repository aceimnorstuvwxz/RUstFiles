
#git remote add origin https://github.com/dgkae/dgkfiles.git

sudo apt-get install -y openssh-server

#git
sudo apt-get install -y git
git config --global user.name "dgkae"
git config --global user.email dgkae.cbf@gmail.com

# VS CODE


#samba
sudo apt-get install samba

#sudo vim /etc/samba/smb.conf
#sudo service smbd restart
#sudo smbpasswd -a smbuser #与 Linux 用户同名
#sudo apt-get install --reinstall libsmbclient libsmbclient-dev libtevent0 libtalloc2
#ps aux | grep smbd
#[homes]
#   comment = Home Directories
#   browseable = yes
#   public = yes
#   read only = no
#   create mask = 0775
#   directory mask = 0775
#   valid users = %S


#oh my zsh
sudo apt-get install -y zsh
sh -c "$(wget https://raw.githubusercontent.com/robbyrussell/oh-my-zsh/master/tools/install.sh -O -)"
