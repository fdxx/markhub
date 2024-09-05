---
## Post Title
title: 'Hello World'

## Default value: config.site.author
author: 'fdxx'

## description. Default value: title
## If the body does not have a '<!-- more -->' , this value is also used as excerpt
description: 'some description'

## Published date. Default value: file last modified date.
date: '2024-09-01'

## last modified date. Default value: date
lastmod: '2024-09-05'

## Enables comment feature for the post. Default value: true
comments: true

## Tags. Default value: none
tags:
  - 'test1'
  - 'test2'

## After building, these files are copied to the corresponding post directory.
## If it is a relative path, it will be find from the current md file path.
resources:
  - 'pexels-dreamypixel-547115.jpg'
  - 'pexels-lazarevkirill-9801136.jpg'

## If it is a draft, it will not be output. Default value: false
draft: false
---

This is a markdown example website.<!-- more --> [Source code](https://raw.githubusercontent.com/fdxx/markhub/main/example-site/source/posts/helloworld/helloworld.md) of this article.

## Paragraphs
**Markdown** is a lightweight markup language with concise syntax, allowing people to focus more on the content itself rather than the layout. It uses a plain text format that is easy to read and write to write documents, can be mixed with HTML, and can export HTML, PDF, and its own .md format files. Because of its simplicity, efficiency, readability, and ease of writing, `Markdown` is widely used, such as [Github](https://github.com/), Wikipedia, Jianshu, etc.

## Emphasis
I just love **bold text**.   
Italicized text is the *cat's meow*.   
<ins>Underline</ins> requires `<ins>` HTML syntax    
~~Delete this text.~~   
Link to the [github.](https://github.com)   

## Quoting text
> **Markdown** is a lightweight markup language with concise syntax, allowing people to focus more on the content itself rather than the layout. It uses a plain text format that is easy to read and write to write documents, can be mixed with HTML, and can export HTML, PDF, and its own .md format files. Because of its simplicity, efficiency, readability, and ease of writing, `Markdown` is widely used, such as [Github](https://github.com/), Wikipedia, Jianshu, etc.

## Lists

### Ordered Lists
1. First item
2. Second item
3. Third item
4. Fourth item
	
### Unordered Lists
- First item
- Second item
- Third item
- Fourth item

### Task lists
- [x] https://github.com/microsoft/vscode/issues/224902
- [ ] Add delight to the experience when all tasks are complete

## Table
| Left-aligned | Center-aligned | Right-aligned |
| :---         |     :---:      |          ---: |
| git status   | git status     | git status    |
| git diff     | git diff       | git diff      |

## Code blocks

```
#include <iostream>

int main(int argc, char *argv[]) {

	/* An annoying "Hello World" example */
	for (auto i = 0; i < 0xFFFF; i++)
		cout << "Hello, World!" << endl;

	char c = '\n';
	unordered_map <string, vector<string> > m;
	m["key"] = "\\\\"; // this is an error

	return -2e3 + 12l;
}
```

```go
package main

import "fmt"

func main() {
	ch := make(chan float64)
    ch <- 1.0e10    // magic number
	x, ok := <- ch
	defer fmt.Println(`exitting now\`)
	go println(len("hello world!"))
	return
}
```

```bash
#!/bin/bash

###### CONFIG
ACCEPTED_HOSTS="/root/.hag_accepted.conf"
BE_VERBOSE=false

if [ "$UID" -ne 0 ]
then
 echo "Superuser rights required"
 exit 2
fi

genApacheConf(){
 echo -e "# Host ${HOME_DIR}$1/$2 :"
}

echo '"quoted"' | tr -d \" > text.txt
```

```json
[
	{
		"title": "apples",
		"count": [12000, 20000],
		"description": {"text": "...", "sensitive": false}
	},
	{
		"title": "oranges",
		"count": [17500, null],
		"description": {"text": "...", "sensitive": false}
	}
]
```

## Footnotes
Here is a simple footnote[^1].  
A footnote can also have multiple lines[^2].

[^1]: My reference.
[^2]: To add line breaks within a footnote.

## Collapsed sections
<details>
<summary>Click to expand</summary>
You can temporarily obscure sections of your Markdown by creating a collapsed section that the reader can choose to expand. For example, when you want to include technical details in an issue comment that may not be relevant or interesting to every reader, you can put those details in a collapsed section.
</details>

## Emoji
😂🤣😡📱🇨🇳😍

## Tabs
::: tabs
@tab:active apple
This is Apple!
@tab banana
This is Banana!
@tab orange
This is Orange!
:::

::: tabs
@tab aa
aaa
@tab:active bb
bbb
@tab cc
ccc
:::

## Alerts
> [!NOTE]
> Useful information that users should know, even when skimming content.

> [!TIP]
> Helpful advice for doing things better or more easily.

> [!IMPORTANT]
> Key information users need to know to achieve their goal.

> [!WARNING]
> Urgent info that needs immediate user attention to avoid problems.

> [!CAUTION]
> Advises about risks or negative outcomes of certain actions.

## Images
### Online images
![](https://img.shields.io/badge/shields-test-blue)

![](https://myoctocat.com/assets/images/base-octocat.svg)

### Local images
![](pexels-lazarevkirill-9801136.jpg)
![](pexels-dreamypixel-547115.jpg)

