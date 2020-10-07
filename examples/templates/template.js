var source = readFileAsString("template.jst");
var idx1 = 0;
var idx2 = -1;

do {
	idx2 = source.indexOf("<?", idx1);
	if (idx2 != -1) {
		// Text up to <? is output directly
		print(source.slice(idx1, idx2));

		// Skip the tag
		idx2 += 2;

		// Then find the terminating tag
		idx1 = source.indexOf("?>", idx2);

		// Eval JavaScript
		if (idx1 != -1) {
			eval(source.slice(idx2, idx1));
		} else {
			eval(source.slice(idx2));
		}

		// Skip the tag
		idx1 += 2;
	} else {
		// Print the remainder of the source
		print(source.slice(idx1));
	}
} while (idx2 != -1);


