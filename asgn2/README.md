A simple web server with support for PUT and GET requests. The possible range of responses are as below.
- 200 OK OK\n When a method is Successful
- 201 Created Created\n When a URI’s file is created
- 400 Bad Request Bad Request\n When a request is ill-formatted
- 403 Forbidden Forbidden\n When a URI’s file is not accessible
- 404 Not Found Not Found\n When the URI’s file does not exist
- 500 Internal Server Error Internal Server Error\n When an unexpected issue prevents processing
- 501 Not Implemented Not Implemented\n When a request includes an unimplemented Method
- 505 Version Not Supported Version Not Supported\n When a request includes an unsupported version