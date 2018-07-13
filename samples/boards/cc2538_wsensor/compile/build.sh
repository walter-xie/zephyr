#!/bin/sh

function compile_boot()
{
	pushd ../xmodem_loader/build/
	cmake ..
        make -j 16
        popd
        cp ../xmodem_loader/build/zephyr/zephyr.bin ../target/xloader.bin
        cp ../xmodem_loader/build/zephyr/zephyr.elf ../target/xloader.elf
}

function compile_app()
{
	pushd ../app/build/
	cmake  ..
	#make menuconfig
	make -j 16
	popd
	cp ../app/build/zephyr/zephyr.bin ../target/app.bin

	LZMA_TOOL_PATH=../../../../../lzma1604/C/Util/Lzma
	LZMA_TOOL=$LZMA_TOOL_PATH/lzma
	pushd $LZMA_TOOL_PATH
	if [ ! -e lzma ];then
        	make -f makefile.gcc
	fi
	popd

	CREATE_LOAD_BIN_TOOL_PATH=../tools/create-load-bin/
	CREATE_LOAD_BIN_TOOL=$CREATE_LOAD_BIN_TOOL_PATH/create-load-bin
	pushd $CREATE_LOAD_BIN_TOOL_PATH
	chmod +x build.sh
	./build.sh
	popd

	board_name=ENT2CTLA_2538
	image_magic=0x54CC2538
	#image_type 0:boot; 1:app
	image_type=1
	#boot_type 0:onchip flash
	boot_type=0
	software_id=1
	#compress_type  0:no compress; 1:lzma
	compress_type=1

	$LZMA_TOOL e ../target/app.bin ../target/app.lzma
	$CREATE_LOAD_BIN_TOOL ../target/app.lzma ../target/app_load_lzma.bin $board_name $image_magic $image_type $boot_type $software_id $compress_type
	compress_type=0
	$CREATE_LOAD_BIN_TOOL ../target/app.bin ../target/app_load.bin $board_name $image_magic $image_type $boot_type $software_id $compress_type
}

if [ $# -eq "0" ];then
	compile_boot
	compile_app
elif [ $# -eq "1" -a $1 = "boot" ]; then
	compile_boot
elif [ $# -eq "1" -a $1 = "app" ]; then
	compile_app
fi
