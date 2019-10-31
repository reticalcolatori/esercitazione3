#!/bin/bash

rm ciao.txt

for i in {1..100000}
do
   echo "$i linea di questo file di cui vorrei rimuoverne una sola mediante un programma cliente servitore" >> ciao.txt
done
