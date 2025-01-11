if [ -n "$1" ]; then
    docker build -t mental1104_dev . --build-arg SSH_PRIVATE_KEY=$1
    docker tag mental1104_dev:latest mental1104/dev:latest
    docker-compose down; docker-compose up -d
    #docker push mental1104/dev:latest
else
    echo "USAGE: run.sh ssh_passwd"
    exit 1
fi