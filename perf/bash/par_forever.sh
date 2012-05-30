while true; do 
    export TMP_DIR=run_`date +"%Y%m%d_%H%M%S"`/
    mkdir -p ${TMP_DIR}
    ./par.sh par.config.dennis 
    sleep 60
done