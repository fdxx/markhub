import markdownit from 'markdown-it';
import fs from 'node:fs';
import Path from 'node:path';
import hljs from 'highlight.js';
import { footnote } from '@mdit/plugin-footnote';
import { tasklist } from '@mdit/plugin-tasklist';
import { imgLazyload } from "@mdit/plugin-img-lazyload";
import { alert } from '@mdit/plugin-alert';
import { tab } from '@mdit/plugin-tab';
import { katex } from "@mdit/plugin-katex";
import YAML from 'yaml';
import ejs from 'ejs';
import ArgsParser from 'minimist';
import anchor from 'markdown-it-anchor';


const __dirname = import.meta.dirname;

class Pagination {
	constructor(postTotal, maxPerPage) {
		this.postTotal = postTotal;
		this.maxPerPage = maxPerPage;
		this.maxPerButton = 5;
	}

	GetPageTotal() {
		return Math.ceil(this.postTotal / this.maxPerPage);
	}

	GetPostHead(pageIndex) {
		return pageIndex * this.maxPerPage;
	}

	GetPostTail(pageIndex) {
		const tailIndex = (pageIndex + 1) * this.maxPerPage - 1;
		return Math.min(tailIndex, this.postTotal - 1);
	}

	HavePrevButton(pageIndex) {
		return pageIndex > 0;
	}

	HaveNextButton(pageIndex) {
		const pageTotal = this.GetPageTotal();
		return pageIndex < pageTotal - 1;
	}

	GetLink(pageIndex, typedata, root) {
		if (pageIndex < 0)
			return "";

		if (pageIndex === 0)
			return `${root}${typedata.firstpath}`;

		return `${root}${typedata.path}/${pageIndex + 1}/`;
	}

	GetButtonCount() {
		const pageTotal = this.GetPageTotal();

		if (pageTotal <= this.maxPerButton) {
			return pageTotal;
		}

		return this.maxPerButton;
	}

	GetCurrentPosition(pageIndex) {
		const pageTotal = this.GetPageTotal();

		if (pageTotal <= this.maxPerButton) {
			return pageIndex;
		}

		if (pageIndex < Math.floor(this.maxPerButton / 2)) {
			return pageIndex;
		}

		if (pageIndex > pageTotal - Math.ceil(this.maxPerButton / 2)) {
			return this.maxPerButton - (pageTotal - pageIndex);
		}

		return Math.floor(this.maxPerButton / 2);
	}
}

function HighlighHandle(code, lang) {
	const fmtcode = code.replace(/\t/g, '    ');

	if (lang && hljs.getLanguage(lang))
		return hljs.highlight(fmtcode, { language: lang }).value;
	return hljs.highlightAuto(fmtcode).value;
}

const mdrenderer = markdownit({ html: true, highlight: HighlighHandle })
	.use(footnote)
	.use(tasklist)
	.use(alert)
	.use(tab, { name: "tabs" })
	.use(imgLazyload)
	.use(katex)
	.use(anchor, {
		level: [1,2],
		permalink: anchor.permalink.linkInsideHeader()
	  })


function GetValue(value, defValue) {
	if (value !== undefined)
		return value;
	return defValue;
}

// YYYY-MM-DD
function FormatDate(str, file = "") {
	let date;

	if (str == undefined) {
		date = fs.statSync(file).mtime;
	}
	else if (str == "now") {
		date = new Date();
	}
	else {
		date = new Date(str);
	}

	const options = {
		year: 'numeric',
		month: '2-digit',
		day: '2-digit',
	};
	return date.toLocaleDateString('en-CA', options);
}


function BuildPosts(config, infile, outdir, postinfo) {
	const input = fs.readFileSync(infile, 'utf8');

	const match = input.match(/^---\s*([\s\S]+?)\s*^---\s*([\s\S]*)$/m);
	const frontmatter = (match && match[1]) ? YAML.parse(match[1]) : "";
	const content = (match && match[2]) ? match[2] : "";

	if (!frontmatter || !content) {
		console.log("Failed to parse the file: %s", infile);
		return false;
	}

	postinfo['title'] = frontmatter['title'];
	if (!postinfo['title']) {
		console.log("Failed to parse title: %s", infile);
		return false;
	}

	postinfo['description'] = GetValue(frontmatter['description'], postinfo['title']);
	postinfo['date'] = FormatDate(frontmatter['date'], infile);
	postinfo['lastmod'] = GetValue(frontmatter['lastmod'], postinfo['date']);
	postinfo['lastmod'] = FormatDate(postinfo['lastmod'], infile);
	postinfo['author'] = GetValue(frontmatter['author'], config['site']['author']);
	postinfo['comments'] = GetValue(frontmatter['comments'], true);
	postinfo['tags'] = GetValue(frontmatter['tags'], []);
	postinfo['resources'] = GetValue(frontmatter['resources'], []);
	postinfo['draft'] = GetValue(frontmatter['draft'], false);
	if (postinfo['draft'])
		return false;

	const structured = {};
	structured['@context'] = "https://schema.org";
	structured['@type'] = "BlogPosting";
	structured['headline'] = postinfo['title'];
	structured['datePublished'] = postinfo['date'];
	structured['dateModified'] = postinfo['lastmod'];
	structured['author'] = {};
	structured['author']['@type'] = "Person";
	structured['author']['name'] = postinfo['author'];
	postinfo['structured'] = JSON.stringify(structured, null, 0);

	const moreTagIndex = content.indexOf('<!-- more -->');
	let excerpt = "";
	if (moreTagIndex !== -1)
		excerpt = content.substring(0, moreTagIndex).trim();
	else
		excerpt = postinfo['description'];

	postinfo['excerpt'] = mdrenderer.render(excerpt);
	postinfo['content'] = mdrenderer.render(content);

	const data = {
		site: config['site'],
		navigation: config['navigation'],
		footer: config['footer'],
		waline: config['waline'],
		giscus: config['giscus'],
		pagecounter: config['pagecounter'],
		translate: config['translate'],
		postinfo: postinfo,
	};

	if (postinfo['comments'] && config['waline']['enable']) {
		data['waline']['el'] = "#waline";
		data['waline']['path'] = postinfo['link'];
		data['waline']['lang'] = data['waline']['lang'] || config['site']['language'];
	}

	const template = fs.readFileSync(Path.join(__dirname, "template", "post.ejs"), 'utf8');
	const output = ejs.render(template, data);
	const outfile = Path.join(outdir, "index.html");
	fs.mkdirSync(outdir, { recursive: true });
	fs.writeFileSync(outfile, output);

	for (const value of postinfo['resources']) {
		const from = Path.isAbsolute(value) ? value : Path.join(Path.dirname(infile), value);
		const to = Path.join(outdir, Path.parse(value).base);
		fs.cpSync(from, to, { recursive: true });
	}
	return true;
}

function BuildPagination(config, arrayPostInfo, typedata) {
	const ejsdata = {
		site: config['site'],
		navigation: config['navigation'],
		footer: config['footer'],
		translate: config['translate'],
		typedata: typedata,
		arrayPostInfoPart: [],
		arrayPagination: [],
		PrevPageLink: "",
		NextPageLink: ""
	};

	const pagination = new Pagination(arrayPostInfo.length, typedata['pageSize']);

	for (let i = 0; i < pagination.GetPageTotal(); i++) {

		const arrayPostInfoPart = [];
		let x = 0;
		for (let p = pagination.GetPostHead(i); p <= pagination.GetPostTail(i); p++) {
			arrayPostInfoPart[x++] = arrayPostInfo[p];
		}

		const arrayPagination = [];
		const curPosition = pagination.GetCurrentPosition(i);

		for (let b = 0; b < pagination.GetButtonCount(); b++) {
			const index = i - curPosition + b;
			const numberInfo = {};
			numberInfo['number'] = index + 1;
			numberInfo['link'] = pagination.GetLink(index, typedata, config['site']['root']);
			numberInfo['isCurrent'] = b === curPosition;
			arrayPagination.push(numberInfo);
		}


		ejsdata.arrayPostInfoPart = arrayPostInfoPart;
		ejsdata.arrayPagination = arrayPagination;

		const PrevPage = pagination.HavePrevButton(i) ? i - 1 : -1;
		const NextPage = pagination.HaveNextButton(i) ? i + 1 : -1;
		ejsdata.PrevPageLink = pagination.GetLink(PrevPage, typedata, config['site']['root']);
		ejsdata.NextPageLink = pagination.GetLink(NextPage, typedata, config['site']['root']);

		let outdir = "";
		if (i === 0) {
			outdir = Path.join(config['site']['push_dir'], typedata['firstpath']);
		}
		else {
			outdir = Path.join(config['site']['push_dir'], typedata['path'], `${i + 1}`);
		}

		fs.mkdirSync(outdir, { recursive: true });
		const outfile = Path.join(outdir, "index.html");

		const template = fs.readFileSync(typedata['template'], 'utf8');
		const output = ejs.render(template, ejsdata);
		fs.writeFileSync(outfile, output);
	}
}

function BuildTagsCloud(config, arrayPostInfo) {
	const tagscloud = {};

	for (const value of arrayPostInfo) {
		const postinfo = {};

		if (value['tags'].length == 0) {
			if (!Object.hasOwn(tagscloud, 'none'))
				tagscloud['none'] = [];
			postinfo['title'] = value['title'];
			postinfo['date'] = value['date'];
			postinfo['link'] = value['link'];
			tagscloud['none'].push(postinfo);
			continue;
		}

		for (const tag of value['tags']) {
			if (!Object.hasOwn(tagscloud, tag))
				tagscloud[tag] = [];
			postinfo['title'] = value['title'];
			postinfo['date'] = value['date'];
			postinfo['link'] = value['link'];
			tagscloud[tag].push(postinfo);
		}
	}

	for (const tag in tagscloud) {
		tagscloud[tag].sort((a, b) => new Date(b.date).getTime() - new Date(a.date).getTime());

		const typedata = {
			pageSize: config['site']['pageSize']['archive'],
			title: tag,
			icon: "ri-price-tag-3-line",
			template: Path.join(__dirname, "template", "archive.ejs"),
			path: `tags/${tag}`,
			firstpath: `tags/${tag}/`
		};
		BuildPagination(config, tagscloud[tag], typedata);
	}

	const data = {
		site: config['site'],
		navigation: config['navigation'],
		footer: config['footer'],
		tagscloud: tagscloud,
	};

	const outdir = Path.join(config['site']['push_dir'], "tags");
	fs.mkdirSync(outdir, { recursive: true });
	const outfile = Path.join(outdir, "index.html");
	const template = fs.readFileSync(Path.join(__dirname, "template", "tagscloud.ejs"), 'utf8');
	const output = ejs.render(template, data);
	fs.writeFileSync(outfile, output);
}

function BuildSiteMap(config, arrayPostInfo) {
	const sitemap = [];
	const siteurl = config['site']['url'];

	for (const value of arrayPostInfo) {
		const postinfo = {};
		postinfo['loc'] = `${siteurl}${value['link']}`;
		postinfo['lastmod'] = value['lastmod'];
		sitemap.push(postinfo);
	}

	sitemap.push({ loc: `${siteurl}${config['site']['root']}`, lastmod: FormatDate("now") });

	sitemap.sort((a, b) => {
		return new Date(b.lastmod).getTime() - new Date(a.lastmod).getTime();
	});

	const outfile = Path.join(config['site']['push_dir'], "sitemap.xml");
	const template = fs.readFileSync(Path.join(__dirname, "template", "sitemap.ejs"), 'utf8');
	const output = ejs.render(template, { sitemap: sitemap });
	fs.writeFileSync(outfile, output);
}

function Build404Page(config) {
	const data = {
		site: config['site'],
		navigation: config['navigation']
	};

	const outfile = Path.join(config['site']['push_dir'], "404.html");
	const template = fs.readFileSync(Path.join(__dirname, "template", "404.ejs"), 'utf8');
	const output = ejs.render(template, data);
	fs.writeFileSync(outfile, output);
}

function ParsePostsDir(dirents, config, arrayPostInfo) {
	for (const entry of dirents) {

		const infile = Path.join(entry.parentPath, entry.name);
		if (entry.isDirectory()) {
			const dir = fs.readdirSync(infile, { withFileTypes: true });
			ParsePostsDir(dir, config, arrayPostInfo);
			continue;
		}

		if (!entry.isFile() || !entry.name.endsWith(".md")) {
			continue;
		}

		const name = Path.parse(entry.name).name;
		const outdir = Path.join(config['site']['push_dir'], name);

		const postinfo = {};
		postinfo['link'] = `${config['site']['root']}${name}/`;

		if (arrayPostInfo.some(item => item['link'] === postinfo['link']))
			throw new Error(`Duplicate post file: ${infile}`);

		if (BuildPosts(config, infile, outdir, postinfo))
			arrayPostInfo.push(postinfo);
	}
}

function main() {
	const args = ArgsParser(process.argv.slice(2));
	let config = args['config'] || Path.join(".", "config.yaml");
	config = YAML.parse(fs.readFileSync(config, 'utf8'));

	if (args['noclean'] === undefined) {
        fs.rmSync(config['site']['push_dir'], { recursive: true, force: true });
    }

	const arrayPostInfo = [];
	const dirents = fs.readdirSync(config['site']['posts_dir'], { withFileTypes: true });
	ParsePostsDir(dirents, config, arrayPostInfo);

	arrayPostInfo.sort((a, b) => {
		return new Date(b.date).getTime() - new Date(a.date).getTime();
	});

	let typedata = {
		pageSize: config['site']['pageSize']['home'],
		title: "",
		icon: "",
		template: Path.join(__dirname, "template", "home.ejs"),
		path: `page`,
		firstpath: ""
	};
	BuildPagination(config, arrayPostInfo, typedata);

	typedata = {
		pageSize: config['site']['pageSize']['archive'],
		title: "Archive",
		icon: "ri-archive-line",
		template: Path.join(__dirname, "template", "archive.ejs"),
		path: `archive`,
		firstpath: `archive/`
	};
	BuildPagination(config, arrayPostInfo, typedata);

	BuildTagsCloud(config, arrayPostInfo);
	BuildSiteMap(config, arrayPostInfo);
	Build404Page(config);

	fs.cpSync(Path.join(__dirname, "assets"), Path.join(config['site']['push_dir'], "assets"), { recursive: true });
	for (const value of config['site']['copy_dir']) {
		fs.cpSync(value['source'], value['dest'], { recursive: true });
	}

	console.log("done!");
}

main();
