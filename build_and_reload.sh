cd ~/projects/oot
source ~/.zsh_profile
rm zelda_ocarina_mq_dbg.z64
killall mupen64plus
make setup -j
gmake -j
./run_glide.sh zelda_ocarina_mq_dbg.z64