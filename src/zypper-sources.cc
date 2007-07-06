
#include "zypper.h"
#include "zypper-sources.h"
#include "zypper-tabulator.h"

#include <fstream>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include <zypp/target/store/PersistentStorage.h>
#include <zypp/base/IOStream.h>

#include <zypp/RepoManager.h>
#include <zypp/RepoInfo.h>
#include <zypp/repo/RepoException.h>
#include <zypp/parser/ParseException.h>
#include <zypp/media/MediaException.h>

using namespace std;
using namespace zypp;
using namespace zypp::repo;
using namespace boost;
using namespace zypp::media;
using namespace zypp::parser;

extern ZYpp::Ptr God;
extern RuntimeData gData;
extern Settings gSettings;


static void do_init_repos()
{
  RepoManager manager;
  gData.repos = manager.knownRepositories();

  for (std::list<RepoInfo>::iterator it = gData.repos.begin();
       it !=  gData.repos.end(); ++it)
  {
    RepoInfo repo(*it);

    //! \todo honor command line options/commands
    bool do_refresh = repo.enabled() && repo.autorefresh(); 

    if (do_refresh)
    {
      //! \todo progress reporting
      cout_n << "Refreshing " << repo.alias() << endl;
      manager.refreshMetadata(repo);
    }
  }
}

// ----------------------------------------------------------------------------

void init_repos()
{
  static bool done = false;
  //! \todo this has to be done so that it works in zypper shell 
  if (done)
    return;

  if ( !gSettings.disable_system_sources )
  {
    do_init_repos();
  }

  done = true;
}

// ----------------------------------------------------------------------------

static void print_repo_list( const std::list<zypp::RepoInfo> &repos )
{
  Table tbl;
  TableHeader th;
  th << "#";
  if (gSettings.is_rug_compatible) th << _("Status");
  else th << _("Enabled") << _("Refresh");
  th << _("Type") << _("Name") << "URI";
  tbl << th;

  int i = 1;
  
  for (std::list<RepoInfo>::const_iterator it = repos.begin();
       it !=  repos.end(); ++it)
  {
    RepoInfo repo = *it;
    TableRow tr (gSettings.is_rug_compatible ? 5 : 6);
    tr << str::numstring (i);

    // rug's status (active, pending => active, disabled <= enabled, disabled)
    // this is probably the closest possible compatibility arrangement
    if (gSettings.is_rug_compatible)
    {
      tr << (repo.enabled() ? _("Active") : _("Disabled"));
    }
    // zypper status (enabled, autorefresh)
    else
    {
      tr << (repo.enabled() ? _("Yes") : _("No"));
      tr << (repo.autorefresh() ? _("Yes") : _("No"));
    }

    tr << repo.type().asString();
    tr << repo.alias();
    
    std::set<Url> urls;
    urls = repo.baseUrls();
    for ( RepoInfo::urls_const_iterator uit = urls.begin();
          uit != urls.end();
          ++uit )
    {
      tr << (*uit).asString();
    }
    
    tbl << tr;
  }

  if (tbl.empty())
    cout_n << _("No repositories defined."
        " Use 'zypper addrepo' command to add one or more repositories.")
         << endl;
  else
    cout << tbl;
}

// ----------------------------------------------------------------------------

void list_repos()
{
  RepoManager manager;
  list<RepoInfo> repos;

  try
  {
    repos = manager.knownRepositories();
  }
  catch ( const Exception &e )
  {
    ZYPP_CAUGHT(e);
    cerr << _("Error reading system sources:") << endl
         << e.asUserString() << endl;
    exit(ZYPPER_EXIT_ERR_ZYPP);
  }

  print_repo_list(repos);
}

// ----------------------------------------------------------------------------

void refresh_repos()
{
  RepoManager manager;
  gData.repos = manager.knownRepositories();

  int error_count = 0;
  int enabled_repo_count = gData.repos.size();
  for (std::list<RepoInfo>::iterator it = gData.repos.begin();
       it !=  gData.repos.end(); ++it)
  {
    RepoInfo repo(*it);

    // skip disabled sources
    if (!repo.enabled())
    {
      cout_v << format(_("Skipping disabled repository '%s'")) % repo.alias()
             << endl;
      enabled_repo_count--;
      continue;
    }

    try
    {
      cout_n << _("Refreshing ") << it->alias() << endl;
      manager.refreshMetadata(repo); //! \todo progress reporting

      cout_v << _("Creating repository cache...") << endl;
      manager.buildCache(repo);

      cout_n << _("DONE") << endl << endl;
    }
    catch ( const Exception &e )
    {
      cerr << format(_("Error reading repository '%s':")) % repo.alias()
        << endl << e.asUserString() << endl;
      cerr << format(_("Skipping repository '%s' because of the above error."))
        % repo.alias() << endl;
      // log untranslated message
      ERR << format("Error reading repository '%s':") % repo.alias()
        << endl << e.msg() << endl;
      ERR << format("Skipping repository '%s' because of the above error.")
        % repo.alias() << endl;
      error_count++;
    }
  }

  if (error_count == enabled_repo_count)
    cerr << _("Could not refresh the repositories because of errors.") << endl;
  else if (error_count)
    cerr << _("Some of the repositories have not been refreshed because of error.") << endl;
  else
    cout_n << _("All repositories have been refreshed.") << endl;
}

// ----------------------------------------------------------------------------

static
std::string timestamp ()
{
  time_t t = time(NULL);
  struct tm * tmp = localtime(&t);

  if (tmp == NULL) {
    return "";
  }

  char outstr[50];
  if (strftime(outstr, sizeof(outstr), "%Y%m%d-%H%M%S", tmp) == 0) {
    return "";
  }
  return outstr;
}

// ----------------------------------------------------------------------------

//! \todo handle zypp exceptions
static
int add_repo(const RepoInfo & repo)
{
  RepoManager manager;

  cout_v << format(_("Adding repository '%s'.")) % repo.alias() << endl;
  MIL << "Going to add repository: " << repo << endl;

  try
  {
    manager.addRepository(repo);
  }
  catch (const MediaException & e)
  {
    cerr << _("Problem transfering repository data from specified URL.") << endl;
    ERR << "Problem transfering repository data from specified URL." << endl;
    return ZYPPER_EXIT_ERR_ZYPP;
  }
  catch (const ParseException & e)
  {
    cerr << _("Problem parsing repository data.") << endl;
    ERR << "Problem parsing repository data." << endl;
    return ZYPPER_EXIT_ERR_ZYPP;
  }
  catch (const RepoAlreadyExistsException & e)
  {
    cerr << format("Repository named '%s' already exists.") % repo.alias() << endl;
    ERR << "Repository named '%s' already exists." << endl;
    return ZYPPER_EXIT_ERR_ZYPP;
  }
  catch (const Exception & e)
  {
    ZYPP_CAUGHT(e);
    cerr << e.asUserString() << endl;
    return ZYPPER_EXIT_ERR_BUG;
  }

  cout_n << format(_("Repository '%s' successfully added:")) % repo.alias() << endl;
  cout_n << ( repo.enabled() ? "[x]" : "[ ]" );
  cout_n << ( repo.autorefresh() ? "* " : "  " );
  cout_n << repo.alias() << " (" << *repo.baseUrlsBegin() << ")" << endl;

  MIL << "Repository successfully added: " << repo << endl;

  return ZYPPER_EXIT_OK;
}

// ----------------------------------------------------------------------------

int add_repo_by_url( const zypp::Url & url, const string & alias,
                     const string & type, bool enabled, bool refresh )
{
  RepoManager manager;

  // determine repository type
  RepoType repotype_probed = manager.probe(url);
  RepoType repotype = RepoType::RPMMD;
  if (type.empty())
    repotype = repotype_probed;
  else
  {
    try
    {
      repotype = RepoType(type);

      if (repotype == repotype_probed)
      {
        cout_v << _("Zypper happy! Detected repository type matches the one"
                     " specified in the --type option.");
      }
      else
      {
        cerr << format(_(
            "Warning! Overriding detected repository type '%s' with "
            "manually specified '%s'.")) % repotype_probed % repotype
            << endl;
        WAR << format(
            "Overriding detected repository type '%s' with "
            "manually specified '%s'.") % repotype_probed % repotype
            << endl;
      }
    }
    catch (RepoUnknownTypeException & e)
    {
      string message = _(
        "Unknown repository type '%s'."
        " Using detected type '%s' instead.");
      cerr << format(message) % type % repotype_probed << endl;

      WAR << format("Unknown repository type '%s'."
        " Using detected type '%s' instead.") % type % repotype_probed << endl;

      repotype = repotype_probed;
    }
  }

  RepoInfo repo;
  repo.setAlias(alias.empty() ? timestamp() : alias);
  repo.setType(repotype);
  repo.addBaseUrl(url);
  repo.setEnabled(enabled);
  repo.setAutorefresh(refresh);
  
  return add_repo(repo);
}

// ----------------------------------------------------------------------------

//! \todo handle zypp exceptions
int add_repo_from_file(const std::string & repo_file_url,
                       const tribool enabled, const tribool autorefresh)
{
  //! \todo handle local .repo files, validate the URL
  Url url(repo_file_url);
  RepoManager manager;
  list<RepoInfo> repos = readRepoFile(url);

  for (list<RepoInfo>::iterator it = repos.begin();
       it !=  repos.end(); ++it)
  {
    RepoInfo repo = *it;

    if (!indeterminate(enabled))
      repo.setEnabled(enabled);
    if (!indeterminate(autorefresh))
      repo.setAutorefresh(autorefresh);

    // by default set enabled and autorefresh to true
    if (indeterminate(repo.enabled()))
      repo.setEnabled(true);
    if (indeterminate(repo.autorefresh()))
      repo.setAutorefresh(true);

    add_repo(repo);
  }

  return ZYPPER_EXIT_OK;
}

// ----------------------------------------------------------------------------

template<typename T>
ostream& operator << (ostream& s, const vector<T>& v) {
  std::copy (v.begin(), v.end(), ostream_iterator<T> (s, ", "));
  return s;
}

// ----------------------------------------------------------------------------

template <typename Target, typename Source>
void safe_lexical_cast (Source s, Target &tr) {
  try {
    tr = boost::lexical_cast<Target> (s);
  }
  catch (boost::bad_lexical_cast &) {
  }
}

// ----------------------------------------------------------------------------

static
bool looks_like_url (const string& s) {
  static bool schemes_shown = false;
  if (!schemes_shown) {
    cerr_vv << "Registered schemes: " << Url::getRegisteredSchemes () << endl;
    schemes_shown = true;
  }

  string::size_type pos = s.find (':');
  if (pos != string::npos) {
    string scheme (s, 0, pos);
    if (Url::isRegisteredScheme (scheme)) {
      return true;
    }
  }
  return false;
}

// ----------------------------------------------------------------------------

void remove_repo( const std::string &alias )
{
  RepoManager manager;
  RepoInfo info;
  info.setAlias(alias);
  manager.removeRepository(info);
}

// ----------------------------------------------------------------------------

void rename_repo(const std::string & alias, const std::string & newalias)
{
  RepoManager manager;
  
  RepoInfo repo(manager.getRepositoryInfo(alias));
  repo.setAlias(newalias);
  
  try
  {
    manager.modifyRepository(alias, repo);

    cout_n << format(_("Repository %s renamed to %s")) % alias % repo.alias() << endl;
    MIL << format(_("Repository %s renamed to %s")) % alias % repo.alias() << endl;
  }
  catch (const Exception & ex)
  {
    cerr << _("Error while modifying the repository:") << endl;
    cerr << ex.asUserString();
    cerr << format(_("Leaving repository %s unchanged.")) % alias << endl;

    ERR << "Error while modifying the repository:" << ex.asUserString() << endl;
  }
}

/*
//! rename a source, identified in any way: alias, url, id
void rename_source( const std::string& anystring, const std::string& newalias )
{
  cerr_vv << "Constructing SourceManager" << endl;
  SourceManager_Ptr manager = SourceManager::sourceManager();
  cerr_vv << "Restoring SourceManager" << endl;
  manager->restore (gSettings.root_dir, true /*use_cache*//*);

  Source_Ref src;

  SourceManager::SourceId sid = 0;
  safe_lexical_cast (anystring, sid);
  if (sid > 0) {
    try {
      src = manager->findSource (sid);
    }
    catch (const Exception & ex) {
      ZYPP_CAUGHT (ex);
      // boost::format: %s is fine regardless of the actual type :-)
      cerr << format (_("Source %s not found.")) % sid << endl;
    }
  }
  else {
    bool is_url = false;
    if (looks_like_url (anystring)) {
	is_url = true;
	cerr_vv << "Looks like a URL" << endl;

	Url url;
	try {
	  url = Url (anystring);
	}
	catch ( const Exception & excpt_r ) {
	  ZYPP_CAUGHT( excpt_r );
	  cerr << _("URL is invalid: ") << excpt_r.asUserString() << endl;
	}
	if (url.isValid ()) {
	  try {
	    src = manager->findSourceByUrl (url);
	  }
	  catch (const Exception & ex) {
	    ZYPP_CAUGHT (ex);
	    cerr << format (_("Source %s not found.")) % url.asString() << endl;
	  }
	}
    }

    if (!is_url) {
      try {
	src = manager->findSource (anystring);
      }
      catch (const Exception & ex) {
	ZYPP_CAUGHT (ex);
	cerr << format (_("Source %s not found.")) % anystring << endl;
      }
    }
  }

  if (src) {
    // getting Source_Ref is useless if we only can use an id
    manager->renameSource (src.numericId (), newalias);
  }

  cerr_vv << "Storing source data" << endl;
  manager->store( gSettings.root_dir, true /*metadata_cache*//* );
}
*/
// ----------------------------------------------------------------------------

// #217028
void warn_if_zmd()
{
  if (system ("pgrep -lx zmd") == 0)
  { // list name, exact match
    cout_n << _("ZENworks Management Daemon is running.\n"
              "WARNING: this command will not synchronize changes.\n"
              "Use rug or yast2 for that.\n");
    USR << ("ZMD is running. Tell the user this will get"
        " ZMD and libzypp out of sync.") << endl;
  }
}

// ----------------------------------------------------------------------------

// OLD code
/*
void cond_init_system_sources ()
{
  static bool done = false;
  if (done)
    return;

  if ( geteuid() != 0 ) {
    cerr << _("Sorry, you need root privileges to use system sources, disabling them...") << endl;
    gSettings.disable_system_sources = true;
    MIL << "system sources disabled" << endl;
  }

  if ( ! gSettings.disable_system_sources ) {
    init_system_sources();
  }
  done = true;
} 
*/
// OLD
/*
void init_system_sources()
{
  SourceManager_Ptr manager;
  manager = SourceManager::sourceManager();
  try
  {
    cerr << _("Restoring system sources...") << endl;
    manager->restore(gSettings.root_dir);
  }
//  catch (const SourcesAlreadyRestoredException& excpt) {
//  }
  catch (Exception & excpt_r)
  {
    ZYPP_CAUGHT (excpt_r);
    ERR << "Couldn't restore sources" << endl;
    cerr << _("Failed to restore sources") << endl;
    exit(-1);
  }
    
  for ( SourceManager::Source_const_iterator it = manager->Source_begin(); it !=  manager->Source_end(); ++it )
  {
    Source_Ref src = manager->findSource(it->alias());
    gData.sources.push_back(src);
  }
}
*/
// OLD
/*
void include_source_by_url( const Url &url )
{
  try
  {
    //cout << "Creating source from " << url << endl;
    Source_Ref src;
    src = SourceFactory().createFrom(url, "/", url.asString(), "");
    //cout << "Source created.. " << endl << src << endl;
    gData.sources.push_back(src);
  }
  catch( const Exception & excpt_r )
  {
    cerr << _("Can't access repository") << endl;
    ZYPP_CAUGHT( excpt_r );
    exit(-1);
  }

}
*/

// Local Variables:
// c-basic-offset: 2
// End:
