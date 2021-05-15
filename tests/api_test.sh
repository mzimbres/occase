#!/bin/bash

# Examples
#
# Endpoint: http://localhost:8080/posts/publish
# Request: {"user":"marcelo", "key":"jTGcIm0rBMSP1BJRJNju3085zwgnVs9w", "post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}
# Reply: {"admin_id":"admin-id","cmd":"publish_ack","date":1616858925,"id":"g90ypieyuf","result":"ok"}
#
# Endpoint: http://localhost:8080/posts/search
# Request: {"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}
# Reply: {"posts":[{"avatar":"","date":1616858925,"description":"","ex_details":[1,2,3],"from":"7502d665b94d87d77343a6c9a6421d0a","id":"g90ypieyuf","images":[],"in_details":[1,2,3],"location":[1,2,3,4],"nick":"","on_search":10,"product":[1,2,3,4],"range_values":[1,2,3],"visualizations":10}]}
#
# Endpoint:
# Request:
# Reply:

search='{"post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
publish1='{"user":"marcelo", "key":"jTGcIm0rBMSP1BJRJNju3085zwgnVs9w", "post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
publish2='{"user":"marcelo", "key":"ssjsjs", "post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
visualization='{"cmd":"visualization", "post_id":"8ft01lo6i8"}'

#curl --verbose --header "Content-Type: application/json" --request POST --data "$search" http://localhost:8080/posts/search
#curl --header "Content-Type: application/json" --request POST --data "$search" http://localhost:8080/posts/count 
#curl --header "Content-Type: application/json" --request POST --data '{"user":"ksksksk", "key":"jdjdjddj"}' http://localhost:8080/posts/upload-credit 
curl --header "Content-Type: application/json" --request POST --data '{"user":"e3f2dd45e22f34b41c8d85805e302ade", "key":"jdjdjddj", "post_id":"t7lgwe5jhs"}' http://localhost:8080/posts/delete 
#curl --header "Content-Type: application/json" --request POST --data "$publish1" http://localhost:8080/posts/publish 
#curl --header "Content-Type: application/json" --request POST --data "$publish2" http://localhost:8080/posts/publish 
#curl --header "Content-Type: application/json" --request POST --data "$visualization" http://localhost:8080/posts/visualization

