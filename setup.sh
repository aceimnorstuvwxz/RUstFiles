
#git remote add origin https://github.com/dgkae/dgkfiles.git

sudo apt-get install -y openssh-server

#git
sudo apt-get install -y git
git config --global user.name "dgkae"
git config --global user.email dgkae.cbf@gmail.com

# VS CODE


#oh my zsh
sudo apt-get install -y zsh
sh -c "$(wget https://raw.githubusercontent.com/robbyrussell/oh-my-zsh/master/tools/install.sh -O -)"
