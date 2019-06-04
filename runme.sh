#!/bin/bash
#MAINTAINER erez@shrewdthings.com
ENERGYBENCH_IMG='energybench-img'
ENERGYMON_IMG='energymon-img'
ENERGYMON_INST='energymon-inst'

ROOTDIR=$PWD

docker_cleanup() {

	docker volume rm $(docker volume ls -qf dangling=true)
	#docker network rm $(docker network ls | grep "bridge" | awk '/ / { print $1 }')
	docker rmi $(docker images --filter "dangling=true" -q --no-trunc)
	docker rmi $(docker images | grep "none" | awk '/ / { print $3 }')
	docker rm $(docker ps -qa --no-trunc --filter "status=exited")
	sleep 1
}

cd $ROOTDIR/energybench
docker build -t $ENERGYBENCH_IMG ./
cd -
cd $ROOTDIR/energymon
docker build -t $ENERGYMON_IMG ./
cd -

docker kill $ENERGYMON_INST
docker_cleanup
modprobe msr
docker run \
	--name $ENERGYMON_INST \
	-td \
	--rm \
	--pid=host \
	--privileged \
	--cap-add=ALL \
	-v /dev:/dev \
	-v /lib/modules:/lib/modules \
	$ENERGYMON_IMG /bin/bash
#docker exec -ti $DOCKER_INST /bin/bash
