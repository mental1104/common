if [ -n "$1" ]; then
    # cp -rf ~/.config/nvim ./devops/INSTALLROOT/root/.config/nvim
    # cp -rf ~/.local/share/nvim ./devops/INSTALLROOT/root/.local/share

    docker build -t mental1104_dev . --build-arg SSH_PRIVATE_KEY=$1; # --no-cache避免使用构建缓存
    docker tag mental1104_dev:latest mental1104/dev:latest;
    docker-compose down; docker-compose up -d;
    # docker push mental1104/dev:latest;
else
    echo "USAGE: run.sh ssh_passwd"
    exit 1
fi
