#!/bin/bash

rm -rf model
mkdir model
pushd model

for url in $(../fetch_model_urls.py)
do
    curl $url -O
done

../reassemble_gguf.sh model_q4km.gguf

popd
