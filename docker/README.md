# Docker support

If you have [Docker]("https://www.docker.com/") installed, you can build a Docker image and run it in a container

```
cd docker
docker build -t ejdb2 .
docker run -d -p 9191:9191 --name myEJDB ejdb2 --access myAccessKey
```

or get an image of `ejdb2` directly from the Docker Hub

```
docker run -d -p 9191:9191 --name myEJDB softmotions/ejdb2 --access myAccessKey
```