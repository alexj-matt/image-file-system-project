## Mandatory informations:
- We have implemented everything said in the handout
- We didn't implemented anything outside of the scope
- In case server is blocked at startup (URL not shown in terminal), Ctrl+C and start again.
- This is a really cool project, the difficulty was appropriate and THANKS for the quality of the first part correction

## Description and Commands:
### General description:
The executable imgfs_server runs a simple IMaGe File System http-server locally. An imgfs file is required as argument and optionally a port number for the server to run on can be specified.
It has the functionality to list the contents of the file system, insert and delete images from the file system and display (read) images in different resolutions: original, small and thumbnail.

### Usage:
Start image server:
	`$ ./imgfs_server <imgfs file> <optional port #>`

Interact through browser:
URL

`http://localhost:<port #>/` (read, insert, delete)

`http://localhost:<port #>/imgfs/list`	(list)

Interact through curl:

`$ curl -i 'http://localhost:<port #>/imgfs/read?res=<resolution>&img_id=<img ID>'`

resolution being any of "orig", "small", "thumb" and img ID being the unique image identifier

`$ curl -i 'http://localhost:<port #>/imgfs/list'`
