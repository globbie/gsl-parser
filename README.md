# gsl-parser
General Semantics Language Parser

GSL is a logical markup language designed within the Knowdy project to provide a human-readable,
compact storage of conceptual graphs. It is used for data storage, message passing and information exchange. The format is not so excessively verbose as XML and even slightly
more   compact   than   JSON. The   language   takes   some
features   of   S-expressions  of   Lisp,   but   with   major
modification of semantics since one should keep in mind
that graphs are not lists. GSL-notation allows its users to express not only the
hierarchical grouping of objects but also the transactional operations within
a database storage system.

## Quick start with GSL

Curly braces are used to delimit a scope or an object.

Empty object:

>{}

An area can be labeled with a tag that starts right after the opening brace
and continues up to the first space character or any other reserved character.
String starting right after the trailing spaces is called a value of a default attribute.

>{class Recipe}

In the example code above *class* is a tag,
and *Recipe* is a value of a default attribute.
In this context it can be *_name*.

Objects can be nested, naturally.

> {class Secret Agent {instance James Bond }}


