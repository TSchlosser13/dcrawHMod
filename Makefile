# Makefile


CC        = gcc
CFLAGS    = -march=native -Ofast -pipe -std=c99 -Wall
p         = _HMod/CHIP
includes  = _HMod/dcrawHMod.c
includes += $(p)/CHIP/Hexint.c $(p)/CHIP/Hexarray.c $(p)/CHIP/Hexsamp.c
includes += $(p)/Misc/pArray2d.c $(p)/Misc/Precalcs.c
includes += $(p)/HFBs/HFBs.c
LDFLS     = -lm -lpthread -DNODEPS `pkg-config opencv --cflags --libs`


.PHONY: dcrawHMod

dcrawHMod: dcraw.c
	$(CC) $(CFLAGS) dcraw.c -o dcrawHMod $(includes) $(LDFLS)

help:
	@echo "/*****************************************************************************"
	@echo " * help : dcrawHMod - v1.0 - 18.07.2016"
	@echo " *****************************************************************************"
	@echo " * + update  : Aktualisierung abhängiger Repos (CHIP)"
	@echo " * + test    : Testlauf"
	@echo " * + testset : Erzeugung Testset (siehe \"Hex-Player/Testset\")"
	@echo " * + clean   : Aufräumen"
	@echo " *****************************************************************************/"

clean:
	find . -maxdepth 1 ! -name "*.c" ! -name "*.man" ! -name "583A0735.cr2" \
		! -name "COPYING" ! -name "Makefile" -type f -delete

_update:
	git pull
	@if [ -d _HMod/CHIP ]; then \
		cd _HMod/CHIP;         \
		echo "CHIP: git pull"; \
		git pull;              \
		cd ../..;              \
	else \
		cd _HMod; \
		git clone https://github.com/TSchlosser13/CHIP CHIP; \
		cd ..; \
	fi

update: _update dcrawHMod

test:
	./dcrawHMod -v -q 0 HMod 11 7 1.0 HGC 583A0735.cr2

testset:
	# Hexagonal Interpolation
	./dcrawHMod -v -q 0 HMod 0 5 1.0 HGC 583A0735.cr2
	mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_0.dat
	echo "HA-HMod_0.dat" >  Hex-Player/Testset/_load_me.dat
	./dcrawHMod -v -q 0 HMod 1 5 1.0 HGC 583A0735.cr2
	mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_1.dat
	echo "HA-HMod_1.dat" >> Hex-Player/Testset/_load_me.dat
	for i in 2 3 4 5; do \
		./dcrawHMod -v -q 1 HMod $$i 5 1.0 HGC 583A0735.cr2; \
		mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_$$i.dat; \
		echo "HA-HMod_$$i.dat" >> Hex-Player/Testset/_load_me.dat; \
	done
	for i in 6 7 8 9 10 11; do \
		./dcrawHMod -v -q 0 HMod $$i 5 1.0 HGC 583A0735.cr2; \
		mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_$$i.dat; \
		echo "HA-HMod_$$i.dat" >> Hex-Player/Testset/_load_me.dat; \
	done
	# Hexagonal White Balance (HWB)
	for i in 0 1 2 3 4 5 6 7; do \
		./dcrawHMod -v -q 0 HMod 11 5 1.0 HWB $$i 583A0735.cr2; \
		mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_11-HWB_$$i.dat; \
		echo "HA-HMod_11-HWB_$$i.dat" >> Hex-Player/Testset/_load_me.dat; \
	done
	./dcrawHMod -v -q 0 HMod 11 5 1.0 HWB 8 96.0 583A0735.cr2
	mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_11-HWB_8_96.0.dat
	echo "HA-HMod_11-HWB_8_96.0.dat" >> Hex-Player/Testset/_load_me.dat
	# Hexagonal Filter Banks (HFBs)
	./dcrawHMod -v -q 0 HMod 11 5 1.0 HFBs 2.0 1.1 583A0735.cr2
	mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_11-HFBs_1.1.dat
	echo   "HA-HMod_11-HFBs_1.1.dat" >> Hex-Player/Testset/_load_me.dat
	./dcrawHMod -v -q 0 HMod 11 5 1.0 HFBs 2.0 0.9 583A0735.cr2
	mv 583A0735-HIP.dat Hex-Player/Testset/HA-HMod_11-HFBs_0.9.dat
	printf "HA-HMod_11-HFBs_0.9.dat" >> Hex-Player/Testset/_load_me.dat
