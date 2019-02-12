#!/usr/bin/env bash

for stub in *
do
	if [[ -d "$stub" ]]
	then
		echo ${stub}
		rm ${stub}/newcomair_123456789
		rm ${stub}/build/*
		rm ${stub}/results/*
		rm ${stub}/targets/*
	fi
done
