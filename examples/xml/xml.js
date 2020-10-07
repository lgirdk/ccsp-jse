var myObject = {
	name: "Eric",
	age: 69,
	height: 175,
	skills: [
		'JavaScript', 'C', 'C++', 'Java', 'PHP', 'ARM Assembler'
	],
        address: [{
		house: 69,
		street: "Acacia Avenue",
		city: "Anywhere",
		Country: "UK"
	}, {
		house: "221b",
		street: "Baker Street",
		city: "Anywhere",
		Country: "UK"
	}]
};

print(objectToXMLString("person", myObject));


