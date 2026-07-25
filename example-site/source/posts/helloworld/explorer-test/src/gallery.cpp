#include <ImgSize.hpp>
#include <cstdio>
#include <inja.hpp>
#include <JustifiedLayout.hpp>
#include <ArgsParser.hpp>

#include <print>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <dirent.h>

namespace fs = std::filesystem;
namespace jl = JustifiedLayout;
using json = nlohmann::json;

static constexpr double MAXWIDTH  = 1800.0;
static constexpr double MAXHEIGHT = 500.0;
static const char *ASSETSDIR = "assets";
static const std::unordered_set<std::string> IMAGE_EXTS = {
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".tiff", ".tif"
};

template<typename... Args>
static void Print(std::FILE *f, std::string_view fmt, Args&&... args)
{
    using namespace std::chrono;
    zoned_time zt{current_zone(), floor<seconds>(system_clock::now())};
    std::string msg = std::vformat(fmt, std::make_format_args(args...));
    std::print(f, "{:%F %T} {}\n", zt, msg);
    std::fflush(f);
}

class FileCounter
{
public:
    FileCounter(const fs::path &p)
    {
        m_Dir = p;
        m_lastCount = 0;
        std::ifstream file(SAVE_FILE);
        if (file)
            file >> m_lastCount;
    }

    bool IsChanged()
    {
        size_t curCount = CalcuFiles(m_Dir);
        if (curCount != m_lastCount)
        {
            m_lastCount = curCount;
            //持久化
            std::ofstream(SAVE_FILE, std::ios::trunc) << m_lastCount;
            return true;
        }
        return false;
    }

    size_t GetLastCount()
    {
        return m_lastCount;
    }

private:
    size_t CalcuFiles(const fs::path& p)
    {
        DIR *dir = ::opendir(p.c_str());
        if (!dir)
        {
            Print(stderr, "Error: opendir failed: {}.", p.c_str());
            std::abort();
        }

        size_t count = 0;
        struct dirent *entry;
        while ((entry = ::readdir(dir)))
        {
            if (entry->d_name[0] == '.')
                continue;
            count++;
            if (entry->d_type == DT_DIR)
                count += CalcuFiles(p / entry->d_name);
        }
        ::closedir(dir);
        return count;
    }

    size_t m_lastCount;
    fs::path m_Dir;
    static inline const fs::path SAVE_FILE = ".filecount";
};


class GalleryGenerator
{
public:
    struct ImageInfo
    {
        ImageInfo(const fs::path &p, fs::file_time_type t, double r)
            : path(p), mtime(t), ratio(r) {}
            
        fs::path path; 
        fs::file_time_type mtime;
        double ratio;
    };

    struct DirNode
    {
        fs::path              absPath;
        std::vector<DirNode>  subdirs; 
        std::vector<ImageInfo> images;
        std::size_t totalImgs;
    };

    explicit GalleryGenerator(const fs::path &src, const jl::LayoutConfig &layout, const fs::path &out, const fs::path &assets, const fs::path &webroot)
    {
        m_srcRoot = fs::absolute(src);
        m_outRoot = fs::absolute(out);
        m_assets = assets;
        m_albumTemplate = m_assets / "templates" / "album.html";
        m_photoTemplate = m_assets / "templates" / "photo.html";
        m_sentinelFile = m_outRoot / ".gallery_sentine";
        m_srcLink = m_outRoot / "_src";
        m_webRoot = webroot;

        m_LayoutCfg = layout;
        m_injaEnv.set_trim_blocks(true);
        m_injaEnv.set_lstrip_blocks(true);

        if (!fs::is_directory(m_srcRoot))
        {
            Print(stderr, "Error: {} is not a directory.", m_srcRoot.c_str());
            std::abort();
        }

        if (!fs::exists(m_albumTemplate) || !fs::exists(m_photoTemplate))
        {
            Print(stderr, "Error: HTML template does not exist.");
            std::abort();
        }
    }

    void GenSite()
    {
        ClearOutDir();

        fs::create_directories(m_outRoot);
        std::ofstream(m_sentinelFile).close();
        fs::create_symlink(m_srcRoot, m_srcLink);
        fs::copy(m_assets, m_outRoot, fs::copy_options::recursive);
        fs::remove_all(m_outRoot / "templates");

        DirNode rootNode = BuildTree(m_srcRoot);
        GenDir(rootNode);
        Print(stdout, "[end] GenSite done.");
    }

    const fs::path &GetSrcDir() const
    {
        return m_srcRoot;
    } 

    const fs::path &GetOutDir() const
    {
        return m_outRoot;
    }

private:
    void ClearOutDir()
    {
        if (fs::exists(m_outRoot) && !fs::exists(m_sentinelFile))
        {
            Print(stderr, "Error: Output directory lacks sentinel — refusing to delete.");
            std::abort();
        }
        fs::remove_all(m_outRoot);
    }

    DirNode BuildTree(const fs::path& dir)
    {
        DirNode node;
        node.absPath = dir;
        node.images = CollectImages(dir);
        node.totalImgs = node.images.size();

        for (const auto& e : fs::directory_iterator(dir))
        {
            if (!e.is_directory())
                continue;

            auto child = BuildTree(e.path());
            node.totalImgs += child.totalImgs;
            node.subdirs.push_back(std::move(child));
        }

        return node;
    }

    void GenDir(DirNode& node)
    {
        if (node.subdirs.empty())
        {
            GenPhotoPage(node);
            return;
        }

        GenAlbumPage(node);
        for (auto& sub : node.subdirs)
            GenDir(sub);
    }

    void GenAlbumPage(DirNode& node)
    {
        // 按照文件名称排序
        std::sort(node.subdirs.begin(), node.subdirs.end(), [](DirNode& a, DirNode& b) {
            return a.absPath < b.absPath;
        });

        json albums = json::array();
        for (const auto& sub : node.subdirs)
        {
            auto &e = albums.emplace_back();
            e["name"]  = sub.absPath.filename().string();
            e["url"]   = GetWebIndexPath(fs::relative(sub.absPath, m_srcRoot));
            e["count"] = sub.totalImgs;
        }

        auto rel = fs::relative(node.absPath, m_srcRoot);
        json data;
        data["root"]    = m_webRoot.generic_string();
        data["title"]   = (rel.empty() || rel == ".") ? std::string("Photo Gallery") : node.absPath.filename().string();
        data["albums"]  = std::move(albums);
        data["navbar"]  = BuildNavbar(rel);

        fs::path out = m_outRoot / rel;
        fs::create_directories(out);
        std::ofstream ofs(out / "index.html");
        ofs << m_injaEnv.render_file(m_albumTemplate, data);
    }

    void GenPhotoPage(DirNode& node)
    {
        // 按照文件最后修改日期排序
        std::sort(node.images.begin(), node.images.end(), [](const ImageInfo& a, const ImageInfo& b){
            return a.mtime > b.mtime;
        });

        // Build justified layout
        std::vector<jl::InputItem> items;
        for (const auto& img : node.images)
            items.emplace_back(img.ratio);
        auto layout = jl::compute(items, m_LayoutCfg);

        json photos = json::array();
        for (std::size_t i = 0; i < node.images.size(); ++i)
        {
            const auto& box = layout.boxes[i];
            json &p = photos.emplace_back();

            p["src"]    = GetWebImgPath(fs::relative(node.images[i].path, m_srcRoot));
            p["name"]   = node.images[i].path.filename().string();
            p["top"]    = (int)std::round(box.top);
            p["left"]   = (int)std::round(box.left);
            p["width"]  = (int)std::round(box.width);
            p["height"] = (int)std::round(box.height);
        }

        auto rel = fs::relative(node.absPath, m_srcRoot);
        json data;
        data["root"]       = m_webRoot.generic_string();
        data["title"]      = node.absPath.filename().string();
        data["count"]      = node.totalImgs;
        data["containerW"] = (int)std::ceil(m_LayoutCfg.containerWidth);
        data["containerH"] = (int)std::ceil(layout.containerHeight);
        data["photos"]     = std::move(photos);
        data["navbar"]     = BuildNavbar(rel);

        auto out = m_outRoot / rel;
        fs::create_directories(out);
        std::ofstream ofs(out / "index.html");
        ofs << m_injaEnv.render_file(m_photoTemplate, data);
    }

    json BuildNavbar(const fs::path &rel) 
    {
        json navbar = json::array();
        fs::path acc;
        for (const auto &part : rel)
        {
            if (part == ".") continue;
            acc /= part;
            auto &crumb = navbar.emplace_back();
            crumb["name"] = part.string();
            crumb["url"]  = GetWebIndexPath(acc);
        }
        return navbar;
    }

    std::vector<ImageInfo> CollectImages(const fs::path& dir)
    {
        std::vector<ImageInfo> imgs;
        for (const auto& e : fs::directory_iterator(dir))
        {
            if (!e.is_regular_file())
                continue;
            if (!IsImage(e.path()))
                continue;
            imgs.emplace_back(e.path(), e.last_write_time(), GetImgRatio(e.path()));
        }
        
        return imgs;
    }

    std::string GetWebImgPath(const fs::path& rel)
    {
        fs::path path = m_webRoot / "_src" / rel;
        return path.generic_string();
    }

    std::string GetWebIndexPath(const fs::path& rel)
    {
        fs::path path = m_webRoot / rel / "index.html";
        return path.generic_string();
    }

    double GetImgRatio(const fs::path &path)
    {
        auto it = m_ImgRatioMap.find(path.generic_string());
        if (it != m_ImgRatioMap.end())
            return it->second;

        auto size = imgsize::from_file(path.c_str());
        if (!size)
        {
            Print(stderr, "Error: Failed to get image info: {}", path.c_str());
            return 0.0;
        }

        double ratio = double(size->width)/size->height;
        m_ImgRatioMap.emplace(path.generic_string(), ratio);
        return ratio;
    }

    bool IsImage(const fs::path& p)
    {
        return IMAGE_EXTS.contains(p.extension().string());
    }


    fs::path m_srcRoot;
    fs::path m_outRoot;
    fs::path m_assets;
    fs::path m_albumTemplate;
    fs::path m_photoTemplate;
    fs::path m_sentinelFile;
    fs::path m_srcLink;
    fs::path m_webRoot;
    
    jl::LayoutConfig m_LayoutCfg;
    inja::Environment m_injaEnv;

    std::unordered_map<std::string, double> m_ImgRatioMap;
};


int main(int argc, char* argv[])
{
    ArgsParser args(argc, argv);
    if (!args.Has("-src") || !args.Has("-out"))
    {
        Print(stderr, "Usage: {} -src=dir -out=dir [-update=0] [-maxw=1800] [-maxh=500] [-assets=assets] [-webroot=/]", args.GetCmdName());
        return 1;
    }

    fs::path srcRoot = args.Get<std::string>("-src");
    fs::path outRoot = args.Get<std::string>("-out");
    fs::path assets = args.Get<std::string>("-assets", ASSETSDIR);
    fs::path webroot = args.Get<std::string>("-webroot", "/");
    int updateInterval = args.Get<int>("-update", 0);

    jl::LayoutConfig LayoutCfg;
    LayoutCfg.containerWidth            = args.Get<double>("-maxw", MAXWIDTH);
    LayoutCfg.targetRowHeight           = args.Get<double>("-maxh", MAXHEIGHT);
    LayoutCfg.targetRowHeightTolerance  = 0.25;
    LayoutCfg.containerPadding          = {0, 0, 0, 0};
    LayoutCfg.boxSpacing                = {6, 6};
    LayoutCfg.showWidows                = true;
    LayoutCfg.widowLayoutStyle          = jl::WidowLayoutStyle::Left;

    FileCounter fCounter(srcRoot);
    GalleryGenerator Gallery(srcRoot, LayoutCfg, outRoot, assets, webroot);

    Print(stdout, "[start] photos={} output={} updateInterval={}(seconds)", srcRoot.c_str(), outRoot.c_str(), updateInterval);
    
    if (updateInterval < 1)
    {
        Gallery.GenSite();
        return 0;
    }

    while (true)
    {
        size_t count = fCounter.GetLastCount();
        if (fCounter.IsChanged() || !fs::exists(Gallery.GetOutDir()))
        {
            Print(stdout, "[update] fileCount changed: {} -> {}", count, fCounter.GetLastCount());
            Gallery.GenSite();
        }

        std::this_thread::sleep_for(std::chrono::seconds(updateInterval));
    }

    return 0;
}


