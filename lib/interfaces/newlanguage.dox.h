/** \file newlanguage.dox.h
  * \brief How to add support for a language in KDevelop
  */

/** \page howToAddNewLanguage How to add support for a language in KDevelop

\section LSupport List of things to have "complete" support of a given language in KDevelop

  - Implement interface KDevlLanguageSupport
  - Language parser for class browser
  - UI subclassing
    - (if the language has Qt bindings)
	.
  - Problem reporter
    - (parses source on the fly and reports syntax errors)
	.
  - Syntax highlighter
    - (add to QEditor if not available elsewhere)
	.
  - Project manager (make/ant/etc)
  - Source file templates
  - Application wizard
  - Source code abbreviations
    - (ife expands to an if else statement, etc)
	.
  - Source formatter (prettyprint functionality)
  - Debugger
    - (gdb/jdb/??? integration)
  - Compiler plugins
  - KDevLang project template
  .

\section Expl Explanations, hints, tips, and all around Good Things To Know, regarding that list

Any language support should be written as a kdevelop part and implement
KDevLanguageSupport interface (<code>lib/interfaces/kdevlanguagesupport.h</code>).

Implementing methods:
 - <code>virtual Features features();</code>
 - <code>virtual KMimeType::List mimeTypes();</code>
 .

Should be enough for a language support to start working.

\section KDevLang KDevLang project template
KDevelop ships with KDevLang project template. It is a simple language support prototype that can be used when developing language support plugins with KDevelop.
To use it, start a New Project and select: <code>C++->KDevelop->KDevelop Language Support Plugin</code> in the application wizard.
The template is located in <code>parts/appwizard/kdevlang</code>, you can change it there if you need.


\section LSupp Language Support

<B>Language support</B> can be written either as a source tree build or as a
standalone part (check out KDevelop part (Standalone build) application
template in KDevelop).

You should look at <code>parts/rubysupport</code> for a simple language support
implementation. For a compilable language support, consult <code>parts/adasupport</code> or <code>parts/pascalsupport</code> (they are not so complex as <code>parts/cppsupport</code>).

Pascal would be a good starting place, as it is the smaller of the two by far.

Language support can offer additional features:
  - new class wizard: (See <code>adasupport, phpsupport, cppsupport</code> or <code>javasupport</code>)
    - <code>virtual void addClass();</code>
    .
  - add method dialog: (See <code>cppsupport</code> or <code>javasupport</code>)
    - <code>virtual void addMethod(const QString &className);</code>
    .
  - add attribute dialog: (See <code>cppsupport</code> or <code>javasupport</code>)
    - <code>virtual void addAttribute(const QString &className);</code>
    .
  .

If there is a Qt bindings for your language and there is a possibility
to use QtDesigner ui files, you could implement ui subclassing feature:
  - <code>virtual QStringList subclassWidget(const QString& formName);</code>
  - <code>virtual QStringList updateWidget(const QString& formName, const QString&
        fileName);</code>
  .

See <code>cppsupport</code> and <code>javasupport</code> for examples.

\section ClStore Class Store

If you write (or have) a language parser, your language support can have
"class store" (a database containing the information about scopes, classes
and methods - their names, names of source files, location in source files,
etc.). Class store libraries can be found at <code>lib/catalog</code> (Catalog) and <code>lib/interfaces</code> (CodeModel).

KDevelop provides class browsers that extract information from a <B>class store</B> and display it in a tree view and toolbar selectors of scopes, classes and methods.

Catalog is the persistant class store for KDevelop. Persistant class store can be used as an information storage for code completion but it also can be used as a class store for the project. Take a look at
<code>parts/cppsupport</code> for an example of using catalog. Catalog is stored on disk in the database file (Berkeley db) If you use catalog with the project, your class browser will be <code>parts/classbrowser</code>.

CodeModel is the memory class store. It is very efficient and thus it is recommended for using as a project class store. CodeModel libraries are located in <code>lib/interfaces/codemodel.h</code>. The class browser for a CodeModel based stores is <code>parts/classview</code>.

Class store enables you to write a code completion for the language. At the
moment (2003-06-25), code completion is available only to cppsupport so take a
look at it for an example.

In general, class stores can be filled with information without specialized
and complex language parsers (take a look at <code>parts/pythonsupport</code> that have a very simple python parser) but your language support will surely benefit
from having such. There is a hand-written c/c++ parser (<code>lib/cppparser</code>) in KDevelop that might be used for ObjC or related C-based languages. Other (not so complex as c++) languages can be parsed by ANTLR based parsers (library is in lib/antlr). Consult www.antlr.org for a ANTLR documentation and look at <code>parts/javasupport</code>, <code>parts/adasupport</code> and <code>parts/pascalsupport</code> for an example of using such parsers. The latest version of ANTLR (2.7.2) has support for Java, C, Pascal, Ada, C++, CIM, HTML, IDL, Verilog, VRML, OCL, ASN.1, and SQL. You can write an ANTLR parser for your own language, of course.

\section PrReport Problem Reporter

If you have a language parser, you can implement <B>problem reporter</B>
functionality for your language. The problem reporter catches errors
reported by a parser and displays it in a problem reporter view.
<code>parts/javasupport</code>, <code>parts/adasupport</code>, <code>parts/pascalsupport</code> and <code>parts/cppsupport</code> have problem reporters.

\section PrManag Project Manager

The language support is important, but it is unusable without a <B>project
manager</B> that can manage projects written on this language. KDevelop
currently provides several project managers. They are:

  - Automake manager
    - <code>parts/autoproject</code>
    .
  - QMake manager
    - <code>parts/trollproject</code>
    .
  - Generic manager
    - Offers project manager facilities using project files in xml format (dtd is located in <code>parts/genericproject/kdevxmlproject.dtd</code>). Those xml files can be converted into makefiles, ant xml files or simply shell scripts using build system plugins. Build system plugin is an object that implements KDevBuildSystem interface. Build system plugins are located in <code>plugins/buildsystem</code>.
  .

Also available:
  - Custom project manager
    - <code>parts/customproject</code>
    - (works with custom makefiles, also has ant support)
    .
  - Script project manager
    - <code>parts/scriptproject</code>
    - (the generic project manager for all scripting languages).
    .
  - <code>parts/pascalproject</code> and
  - <code>parts/adaproject</code>
  - <code>parts/haskellproject</code>
  .

(They are deprecated project managers that will be replaced with generic manager).

Choose your project manager and if the existing project managers doesn't fit
in, extend generic project manager via build system plugin.

\section Templ Templates

\subsection AppWiz Application Wizard Templates

<B>Application wizard</B> templates should be also written. Appwizard templates are simple to create - consult <code>parts/appwizard/README</code> and look at
<code>parts/appwizard/rubyhello</code>, <code>parts/appwizard/pascalhello</code>, or <code>parts/appwizard/adahello</code>.

\subsection FCreateTpl File Create Templates

Another thing to do is to create <B>file create templates</B>. They are prototypes for a source files of your language. These prototypes are placed in
<code>parts/filecreate/file-templates</code> dir and have names equal to the extensions of language source files. The description of the prototypes is placed
in <code>parts/filecreate/template-info.xml</code>. Consult <code>parts/filecreate/README</code> for further information.

\subsection CdAbbr Code Abbreviation Templates

KDevelop has a support for <B>code abbreviations</B> so you can add some predefined abbreviations to your language support. Take <code>parts/cppsupport/cpptemplates</code> as an example.

\section Editor Language Editor

To edit source files KDevelop uses any editor that supports the KTextEditor
interface. In case none of the editors does support advanced editing of sources
written in your language (like code folding, syntax highlighting, line
indentation) you can improve QEditor included in KDevelop (<code>parts/qeditor</code>). By creating QEditorIndenter and QSourceColorizer descendants you can provide the support for an automatic indentation and syntax highlighting that will be available for sure in KDevelop.

To obtain source formatter functionality (that is already available to
c-based languages) you can extend astyle library (<code>lib/astyle</code>) that is used by KDevelop to format sources.

\section Debugger Debugger Support

The last thing to have a complete language support in KDevelop is to
write a <B>debugger support</B>. KDevelop already provides GDB support
(<code>parts/debugger</code>) and JDB (java debugger) support (<code>parts/javadebugger</code>). Take a look at them to get inspiration.

\section Compiler Compiler plugins

There is an ability to create compiler plugin for KDevelop. Compiler plugin provides the compiler configuration dialog which implements command line compiler options. Compiler plugins must implement KDevCompilerOptions interface.

\section MiscInf Other Info

See also <code>HACKING</code> file for an information on how to create a generic KDevelop plugin and how to manage project and global configuration information.

*/
