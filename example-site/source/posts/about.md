---
title: 'About'
date: '2024-08-15'
---

[MarkHub](https://github.com/fdxx/markhub), A simple blog system built with nodejs. <!-- more --> It parses the Markdown file into an HTML file and outputs it to the specified directory. You need to use nginx etc. to present your website.

### [Preview](https://fdxx.github.io/markhub)

## Features
- Easily write articles using markdown syntax.
- github css theme styles.

## build
```bash
git clone https://github.com/fdxx/markhub
cd markhub
node install

## Copy the configuration file and edit it.
cp config.example.yaml config.yaml

## build
node index.js
```

## Command Parameter
- **config**: Specify config file path. default value `./config.yaml`. For config instructions, see `config.example.yaml`.
- **noclean**: By default, all files in `config.site.push_dir` are deleted before building, specify this parameter to prevent deletion of the.

Example:

```bash
node index.js --config "path/to/config.yaml" --noclean
```

## Front-matter
At the beginning of the post, surround the yaml block with `---`
```yaml
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
```
