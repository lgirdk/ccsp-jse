# Templates

This example shows how to parse and execute JavaScript template files
from JavaScript. The example file *template.js* reads *templace.jse*,
parses it and where <? ?> tags exist, it executes the JavaScript 
contained within using the JavaScript *eval()* built in.

In a real life situation you would pass in the template file name
rather than hard code it.

The output of the script is:

```html
<html>
<head>
<title>Test</title>
</head>
<body>
<h1>Hello World</h1>
<h2>A sum</h2>
2 + 3 + 4 + 5 + 6 + 7 + 8 = 35
<h2>Variables</h2>
Declare the variable here!
The variable's value is 69
</body>
</html>
```


