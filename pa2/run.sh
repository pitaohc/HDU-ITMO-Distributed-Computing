export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:./lib64"
export LD_PRELOAD="/home/dc/projects/ITMO-distributed_computing_ans/pa2/lib64/libruntime.so"
./pa2 $*
