docker run -v `pwd`:/libAL:Z kazade/dreamcast-sdk /bin/sh -c "source /etc/bash.bashrc; cd /libAL; make clean && make samples"
