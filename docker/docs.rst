.. _chapter-docker:

------
docker
------
The Dockerfile in this directory simplifies creating a container with cached
CIPD packages for use in continuous integration.

To create a container run ``docker build --tag <tag> $PW_ROOT/docker``. To run
this image run ``docker run -it <tag>``.

Details of Docker use are unfinished.
