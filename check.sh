#!/bin/bash

rm -f ./grading/nyush

make

cp ./nyush ./grading/nyush

cd ./grading/

./gradeit.sh ./myoutputs ./nyush ./refout