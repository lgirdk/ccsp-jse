if (Request.REQUEST_METHOD == "GET") {
	print(
		'<html>' +
		'<head><title>Upload test</title></head>' +
		'<body>' +
		'<h1>File upload</h1>' +
		'<form action="/xml/upload.jse" method="post" enctype="multipart/form-data">' +
		'<input type="file" name="upload" />' +
		'<input type="submit" name="submit" />' +
		'</form>' +
		'</body>'
	);
	setContentType("text/html");
} else
if (Request.REQUEST_METHOD == "POST") {
	var fileContent = readFileAsString(Request.QueryParameters.upload.savepath);

	print(fileContent);
	print("\r\n");

	setContentType("text/plain");
}

