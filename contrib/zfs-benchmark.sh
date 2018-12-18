#!/bin/bash
# vim: tabstop=4 noexpandtab

#
# DEFUSE FIRST!!
# Search for FIXME and remove the "#" or the echos
#

#
# This script uses fio for benchmarking
# http://freecode.com/projects/fio
#
##The fio job file (doit.fio) should look like this:
# [global]
# ioengine=libaio
# direct=1
# rw=${RW}
# bs=${BS}
# size=${SIZE}
# runtime=${RUNTIME}
# ramp_time=${RAMP_TIME}
# iodepth=${IODEPTH}
#
# [/dev/zvol/ppool01/bench]
#
##The other fio job file (doit_bare_tpl.fio) should look like:
# [global]
# ioengine=libaio
# direct=1
# rw=${RW}
# bs=${BS}
# size=${SIZE}
# runtime=${RUNTIME}
# ramp_time=${RAMP_TIME}
# iodepth=${IODEPTH}
# group_reporting
#
# BENCHDEVS
##


OUTPUTFILE=benchmarkdata.csv
export DISKSIZEINBYTES=1000204886016

for p in 12 ; do
	ASHIFT=$p
	for o in metadata ; do
		CACHE=$o
		for n in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
			NRDEVS=$n
			for m in stripe raidz1 stripe2mirror raidz2 stripe3mirror raidz3 ;do
				ARRAYSCHEME=$m
				for l in 64; do
					VOLBLOCKSIZE=$l
					BENCHDEVS=""
					case $ARRAYSCHEME in
						bare)
							#
							# generate bare device string for use in doit_bare_tpl.fio, to test devices parallel without zfs
							#
							case $NRDEVS in
								1)
									BENCHDEVS="[/dev/sda]"
									;;
								2)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]"
									;;
								3)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]"
									;;
								4)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]"
									;;
								5)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]"
									;;
								6)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]"
									;;
								7)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]\n[/dev/sdg]"
									;;
								8)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]\n[/dev/sdg]\n[/dev/sdh]"
									;;
								9)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]\n[/dev/sdg]\n[/dev/sdh]\n[/dev/sdi]"
									;;
								10)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]\n[/dev/sdg]\n[/dev/sdh]\n[/dev/sdi]\n[/dev/sdj]"
									;;
								11)
									BENCHDEVS="[/dev/sda]\n[/dev/sdb]\n[/dev/sdc]\n[/dev/sdd]\n[/dev/sde]\n[/dev/sdf]\n[/dev/sdg]\n[/dev/sdh]\n[/dev/sdi]\n[/dev/sdj]\n[/dev/sdk]"
									;;
							esac
							;;
						stripe2mirror)
							case $NRDEVS in
								2)
									BENCHDEVS="mirror sda sdb"
									;;
								4)
									BENCHDEVS="mirror sda sdb mirror sdc sdd"
									;;
								6)
									BENCHDEVS="mirror sda sdb mirror sdc sdd mirror sde sdf"
									;;
								8)
									BENCHDEVS="mirror sda sdb mirror sdc sdd mirror sde sdf mirror sdg sdh"
									;;
								10)
									BENCHDEVS="mirror sda sdb mirror sdc sdd mirror sde sdf mirror sdg sdh mirror sdi sdj"
									;;
								12)
									BENCHDEVS="mirror sda sdb mirror sdc sdd mirror sde sdf mirror sdg sdh mirror sdi sdj mirror sdk sdl"
									;;
								14)
									BENCHDEVS="mirror sda sdb mirror sdc sdd mirror sde sdf mirror sdg sdh mirror sdi sdj mirror sdk sdl mirror sdm sdn"
									;;
							esac
							;;
						stripe3mirror)
							case $NRDEVS in
								3)
									BENCHDEVS="mirror sda sdb sdc"
									;;
								6)
									BENCHDEVS="mirror sda sdb sdc mirror sdd sde sdf"
									;;
								9)
									BENCHDEVS="mirror sda sdb sdc mirror sdd sde sdf mirror sdg sdh sdi"
									;;
								12)
									BENCHDEVS="mirror sda sdb sdc mirror sdd sde sdf mirror sdg sdh sdi mirror sdj sdk sdl"
									;;
								15)
									BENCHDEVS="mirror sda sdb sdc mirror sdd sde sdf mirror sdg sdh sdi mirror sdj sdk sdl mirror sdm sdn sdo"
									;;
							esac
							;;
						stripe)
							case $NRDEVS in
								1)
									BENCHDEVS="sda"
									;;
								2)
									BENCHDEVS="sda sdb"
									;;
								3)
									BENCHDEVS="sda sdb sdc"
									;;
								4)
									BENCHDEVS="sda sdb sdc sdd"
									;;
								5)
									BENCHDEVS="sda sdb sdc sdd sde"
									;;
								6)
									BENCHDEVS="sda sdb sdc sdd sde sdf"
									;;
								7)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg"
									;;
								8)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh"
									;;
								9)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi"
									;;
								10)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj"
									;;
								11)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk"
									;;
								12)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl"
									;;
								13)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm"
									;;
								14)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn"
									;;
								15)
									BENCHDEVS="sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn sdo"
									;;
							esac
							;;
						raidz1)
							case $NRDEVS in
								3)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc"
									;;
								4)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd"
									;;
								5)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde"
									;;
								6)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf"
									;;
								7)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg"
									;;
								8)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh"
									;;
								9)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi"
									;;
								10)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj"
									;;
								11)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk"
									;;
								12)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl"
									;;
								13)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm"
									;;
								14)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn"
									;;
								15)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn sdo"
									;;
							esac
							;;
						striperaidz2)
							case $NRDEVS in
								8)
									BENCHDEVS="raidz2 sda sdb sdc sdd raidz2 sde sdf sdg sdh"
									;;
								10)
									BENCHDEVS="raidz2 sda sdb sdc sdd sde raidz2 sdf sdg sdh sdi sdj"
									;;
								12)
									BENCHDEVS="raidz2 sda sdb sdc sdd raidz2 sde sdf sdg sdh raidz2 sdi sdj sdk sdl"
									;;
								15)
									BENCHDEVS="raidz2 sda sdb sdc sdd sde raidz2 sdf sdg sdh sdi sdj raidz2 sdk sdl sdm sdn sdo"
									;;
							esac
							;;
						raidz2)
							case $NRDEVS in
								4)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd"
									;;
								5)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde"
									;;
								6)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf"
									;;
								7)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg"
									;;
								8)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh"
									;;
								9)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi"
									;;
								10)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj"
									;;
								11)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk"
									;;
								12)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl"
									;;
								13)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm"
									;;
								14)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn"
									;;
								15)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn sdo"
									;;
							esac
							;;
						raidz3)
							case $NRDEVS in
								6)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf"
									;;
								7)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg"
									;;
								8)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh"
									;;
								9)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi"
									;;
								10)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj"
									;;
								11)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk"
									;;
								12)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl"
									;;
								13)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm"
									;;
								14)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn"
									;;
								15)
									BENCHDEVS="$ARRAYSCHEME sda sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn sdo"
									;;
							esac
							;;
					esac
					#
					# create the pool and zvol if there is an appropriate list of BENCHDEVS, otherwise skip
					#
					if [ ! -z "$BENCHDEVS" -a ! "$ARRAYSCHEME" = "bare" ]; then
						#
						# temporarily enable caching to speed up subsequent zpool destroy:
						#
						# FIXME 3x:
						echo 3 > /proc/sys/vm/drop_caches
						zfs set primarycache=metadata ppool01
						zpool destroy ppool01
						#
						# create zpool and zvol
						#
						# FIXME:
						# zpool create -f ppool01 -m none -o ashift=$ASHIFT $BENCHDEVS || exit 2
						#
						# calculate 90% of array size, to be used in zfs create -V command
						# 90% seems a safe amount to prevent "out of space" errors
						#
						ARRAYSIZEINBYTES=$(zfs get -p -H available -o value ppool01)
						ARRAYSIZE=$(( ((ARRAYSIZEINBYTES/1024/1024)/10)*9 ))
						# FIXME:
						# zfs create -V ${ARRAYSIZE}M -o volblocksize=${VOLBLOCKSIZE}k -o primarycache=$CACHE ppool01/bench || exit 3
					elif [ "$ARRAYSCHEME" = "bare" ]; then
						# do nothing
						((1+1))
					else
						break
					fi
					#
					# prewrite entire zvol to obtain realistic random iops 
					#
					RW=write
					SIZE=100%
					RUNTIME=0
					RAMP_TIME=0
					IODEPTH=512
					BS=1024
					printf "ASHIFT %s; PRIMARYCACHE %s; NRDEVS %s;ARRAYSCHEME %s; VOLBLOCKSIZE %s;BS %s;IODEPTH %s;RW %s;\n" $ASHIFT $CACHE $NRDEVS $ARRAYSCHEME $VOLBLOCKSIZE ${BS} $IODEPTH $RW
					printf "%s;%s;%s;%s;%s;%s;%s;%s;" $ASHIFT $CACHE $NRDEVS $ARRAYSCHEME $VOLBLOCKSIZE ${BS} $IODEPTH $RW >> $OUTPUTFILE
					# FIXME:
					# RW=$RW BS=${BS}k SIZE=$SIZE RAMP_TIME=$RAMP_TIME RUNTIME=$RUNTIME IODEPTH=$IODEPTH NAME="$ARRAYSCHEME $BENCHDEVS" fio --minimal doit.fio >> $OUTPUTFILE
					#
					# then loop through various blocksizes
					#
					for k in 4;do
						BS=$k
						for i in randwrite randread ;do
							RW=$i
							case $i in
								write)
									RUNTIME=0
									RAMP_TIME=0
									SIZE=100%
									if [ "$ARRAYSCHEME" = "bare" ]; then
										#
										# reduce iodepth to 1 for (parallel) single disk benchmarks 
										#
										IODEPTH=1
									else
										IODEPTH=$(((512*1024)/${BS}))
									fi
									;;
								read)
									RAMP_TIME=0
									if [ "$ARRAYSCHEME" = "bare" ]; then
										SIZE=100%
										RUNTIME=60
										IODEPTH=1
									else
										#
										# read less than the amount of referenced data on a zvol,
										# trying to read more will result in unrealistic /dev/zero GB/s speeds
										#
										# FIXME:
										SIZE=$(( ($(zfs get -p -H referenced -o value ppool01/bench)/10)*9 ))
										RUNTIME=0
										IODEPTH=$(((512*1024)/${BS}))
									fi
									;;
								randread)
									RUNTIME=60
									SIZE=100%
									IODEPTH=256
									RAMP_TIME=0
									;;
								randwrite)
									RUNTIME=60
									SIZE=100%
									IODEPTH=256
									RAMP_TIME=0
									;;
							esac
							#
							# keep enough data in flight agains zvol, but no more than asyncio can handle
							#
							if [ $IODEPTH -ge 65536 ]; then
								IODEPTH=65536
							fi
							#
							# run every benchmark 5 times
							#
							for i in 1 2 3 4 5; do
								# FIXME 2x:
								echo 3 > /proc/sys/vm/drop_caches
								sleep 5
								if [ "$ARRAYSCHEME" = "bare" ];then
									printf "%s;%s;%s;%s;%s;%s;%s;%s;" "" "" $NRDEVS $ARRAYSCHEME "" ${BS} $IODEPTH $RW >> $OUTPUTFILE
									printf "ASHIFT %s; PRIMARYCACHE %s; NRDEVS %s;ARRAYSCHEME %s; VOLBLOCKSIZE %s;BS %s;IODEPTH %s;RW %s;\n" "" "" $NRDEVS $ARRAYSCHEME "" ${BS} $IODEPTH $RW
									sed "s#BENCHDEVS#${BENCHDEVS}#g" doit_bare_tpl.fio > doit_bare.fio
									# FIXME:
									# RW=$RW BS=${BS}k SIZE=$SIZE RAMP_TIME=$RAMP_TIME RUNTIME=$RUNTIME IODEPTH=$IODEPTH fio --minimal --name "$ARRAYSCHEME" doit_bare.fio >> $OUTPUTFILE
								else
									printf "%s;%s;%s;%s;%s;%s;%s;%s;" $ASHIFT $CACHE $NRDEVS $ARRAYSCHEME $VOLBLOCKSIZE ${BS} $IODEPTH $RW >> $OUTPUTFILE
									printf "ASHIFT %s; PRIMARYCACHE %s; NRDEVS %s;ARRAYSCHEME %s; VOLBLOCKSIZE %s;BS %s;IODEPTH %s;RW %s;\n" $ASHIFT $CACHE $NRDEVS $ARRAYSCHEME $VOLBLOCKSIZE ${BS} $IODEPTH $RW
									# FIXME:
									# RW=$RW BS=${BS}k SIZE=$SIZE RAMP_TIME=$RAMP_TIME RUNTIME=$RUNTIME IODEPTH=$IODEPTH NAME="$BENCHDEVS" fio --minimal doit.fio >> $OUTPUTFILE
								fi
								
							done
							# FIXME:
							# zpool status -v ppool01
						done
					done
				done
			done
		done
	done
done
