#!/bin/bash

search='{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "click":20, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}'
publish1='{"user":"marcelo", "key":"jTGcIm0rBMSP1BJRJNju3085zwgnVs9w", "post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "click":20, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
publish2='{"user":"marcelo", "key":"ssjsjs", "post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "click":20, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
visualizations='{"cmd":"visualizations", "post_ids":["mama","kkee"]}'
click='{"cmd":"click", "post_id":"mama"}'

curl --header "Content-Type: application/json" --request POST --data "$search" http://localhost:8080/posts/search
curl --header "Content-Type: application/json" --request POST --data "$search" http://localhost:8080/posts/count 
curl --header "Content-Type: application/json" --request POST --data '{"user":"ksksksk", "key":"jdjdjddj"}' http://localhost:8080/posts/upload-credit 
curl --header "Content-Type: application/json" --request POST --data '{"user":"ksksksk", "key":"jdjdjddj", "post_id":"kuwpsks"}' http://localhost:8080/posts/delete 
curl --header "Content-Type: application/json" --request POST --data "$publish1" http://localhost:8080/posts/publish 
curl --header "Content-Type: application/json" --request POST --data "$publish2" http://localhost:8080/posts/publish 
curl --header "Content-Type: application/json" --request POST --data "$visualizations" http://localhost:8080/posts/visualizations 
curl --header "Content-Type: application/json" --request POST --data "$click" http://localhost:8080/posts/click 

