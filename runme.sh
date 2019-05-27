#!/bin/bash
#MAINTAINER erez@shrewdthings.com
DOCKER_IMG='energy-languages-img'
DOCKER_INST='energy-languages-inst'

docker_cleanup() {

	docker volume rm $(docker volume ls -qf dangling=true)
	#docker network rm $(docker network ls | grep "bridge" | awk '/ / { print $1 }')
	docker rmi $(docker images --filter "dangling=true" -q --no-trunc)
	docker rmi $(docker images | grep "none" | awk '/ / { print $3 }')
	docker rm $(docker ps -qa --no-trunc --filter "status=exited")
}

docker build -t $DOCKER_IMG ./
docker_cleanup
docker kill $DOCKER_INST
docker run --name $DOCKER_INST -td --rm $DOCKER_IMG /bin/bash
docker exec -ti $DOCKER_INST /bin/bash
