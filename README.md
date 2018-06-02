# Watagashi
Watagashi is negative list based c/c++ build system.
The supporting OS is Linux.

# Build
To build this project, prepare the following.
- The compiler which supports c++17
- Boost 1.66.0 or greater

Please execute the following commands to build.
```
git clone https://github.com/tositeru/watagashi.git path/to/clone
cd path/to/clone
make
_watagashi -p watagashi
_watagashi -p watagashi install --output path/to/install/watagashi
make clean
```

# Usage
Watagashi can create config file (.watagashi) interactively to use "create" task.
```
watagashi create
> project name > test
> project type (exe, static, shared) > exe
> use compiler > clang++
> target directory > src
> target extension (exit by enter '.') > cpp
> target extension (exit by enter '.') > .
> config filepath > test.watagashi
```

The config file is JSON.  
The main content in the config file is "Project" and "BuildSetting".
- Project
  - config build target.
  - have multiple BuildSetting.
- BuildSetting
  - config build detail (target directories, compile options, link libraries, and etc).
  - enable specify preprocess and postprocess.
  - output config specify "Output Config" in this. 

Watagashi detect source files with directories and extension.  
To specify directories use "targetDirectories" in Build Setting.  
To specify extensions use "fileFilters" in Target Directory.

For example, to compile file which has "cpp" to extension in "src" directory, describe it as follows.
```
{
  "projects": [ {
    "name": test,
    "type": "exe",
    "buildSettings": [ {
      "name": "default",
      "compiler": "clang++",
      "targetDirectories": [{
        "path": "src", // <- specify directory
        "fileFilters": [ {
          "extensions": ["cpp"] // <- specify extension in directory
        } ]
      } ]
    } ],
    "outputConfig": {
      "name": test
    }
  }]
}
```

When a source file not to want to compile exist in directory, please describes "ignores" in "TargetDirectory".
```
...
  "targetDirectories": [ {
    "path": "src",
    "ignores": ["ignoreFile.cpp"], // <- "ignoreFile.cpp" be ignored
    ...
  } ]
...
```

Complete to edit, let's build!
```
watagashi --config build.watagashi --project test --build-setting default build
# Even a command below is OK
watagashi -c build.watagashi -p test -b default # can omit "build"
watagashi -p test -b default # when config file name is "build.watagashi", can omit --config option
watagashi -p test            # when BuildSetting name is "default", can omit --build-setting option
```

if success to build, the output file can copy by "install" task.
```
watagashi install --output path/to/copy
```

The explanation of basic usage is over.  
Of course, supports "clean" and "rebuild" task.  
Watagashi determine whether you compile a file by the update date and time automatically.
It consider the include files. But the files in system include path ignores.
when those file edited, please rebuild.

Enjoy programming!

# License
MIT

# Advance Usage

## Regular Expression
You can use regular expression at part elements. ex) "path" of TargetDirectory, "extensions" of FileFilter or others.
If you want to use regular expression, please add '@' to an prefix.
```
...
"targetDirectories": [{
	"path": "@src|libSrc", // <- search files in "src" and "libSrc" directory.
	...
}]
...
```

## List Up Task
You display files for the build by using "listup" task.
```
watagashi -p test listup
```
And, you can confirm files for the build when you used the regular expression if you use "check-regex" option.
```
watagashi -p test listup --check-regex \d+\.cpp
```

## Dependence Relationship Between Project
Watagashi can appoint dependence by describing "\<project name\>.\<buildSetting name\>" in "dependences" of "BuildSetting".
When build project, it build dependence project earlier if a project of the dependence is non-update.
```
"buildSettings": [{
	...
	"dependences": ["other_project.default"],
	...
}]
```

## Process
Watagasi execute to preprocess before compile source files, preprocess before link or postprocess.
Please describle charactor string to express a command in "preprocess", "linkPreporcess" or "postprocess" of "BuildSetting" to set it.
The current path of the command becomes the directory of the configuration file (.watagashi).
```
"buildSettings": [{
	"preprocess": "echo run preprocess!",
	"linkPreprocess": "echo run link preprocess!",
	"postprocess": "echo run postprocess!",
}],
...
```

If you want to execute those process in the compile time of a certain file, please describe "FileToProcess" in "FileFilter".
```
"FileFilters": [{
	"extensions": ["cpp"]
	"fileToProcesses": [{
		"filepath": "main.cpp", // <- enable regular expression
		"preprocess": "echo preprocess of main.cpp",
		"postprocess": "echo postprocess of main.cpp"
	}]
}]
```

## Variable
You can use the variable for each item of the configuration file.
You write the variable like '${...}'.
```
...
"preprocess": "echo output name is ${OUTPUT_NAME}",
...
```
We call a variable to be usable without previous preparations a system variable.
The usable system variable cahnges by a hierarchy to call.
The low-order hierarychy can use the variable of a high-order hierarchy belonging to.

And, You can refer to some items in the configuration file like a variable.
You write the variable like '${\<project name\>.XXX}' or '${\<project name\>.\<buildSetting name\>.XXX}'.
```
...
"preprocess": "echo ${otherProject.release.outputFilepath}",
...
```
(By the way, there are still few kinds of the system variable.)

Of course you can define the variable freely using "defineVariables".
You can define "defineVariables" at all hierarchies.
```
...
"defineVaraibles": [
  {"Var1", "Apple"},
  {"Var2", "Orange"}
  {"Var3", "Grape"}
],
"preprocess": "echo ${Var1}",
...
```
In addition, you can define the variable from command line.
In that case, please divide the name and contents in '=' by all means.
```
watagashi -p test -V Global1=:-) -V Global2=:-<
```
The variable is evaluated from command line with the highest priority.
And, the remainder is evaluated from the low-order hierarychy.

## Template
Some hierachies can use a template data by using "template" item.
You use "defineTemplates" for definition of template data.
The definition of "defineTemplates" is possible from any hierarchy.
But, the evaluation of template data finds from the high hierarchy.
```
...
"defineTemplates": [{
  "name": "commonBuildSetting",
  "itemType": "project", // <- template data type
  "compiler": "clang++",
  "targetDirectories": [{
    "path": "src",
  }]
}],
"targetDirectories": [{
  "name": "default",
  "template": "commonBuildSetting",
  "compileOptions": ["-g", "-O0"]
  ...
}, {
  "name": "release",
  "template": "commonBuildSetting",
  "compileOptions": ["-O3"]
  ...
}],
...
```
The item of the hierarchy using the template is set in the template's item.
Of course you can set newly the item which template data don't have.
When you already set the item in template data, as follows by an item.
- overrwrite
- append
- ignore

When a variable is used in template data, a variables is evaluated after the template was evaluated.

# Custom Compiler
Watagashi can customize the compiler. This compiler call the custom compiler.
The custom compiler be defined "customCompiler" of "RootConfig".
Please define the custom compiler in "customCompilers" of "RootConfig".
Please appoint the name in "compiler" of "BuildSetting" to use the compiler which you defined.

You are necessary to input the following items to customize compiler.
- name
- config each project type (exe, static, shared)
	- config compile .obj and link .obj files
		- used command
		- input option
		- output option
		- etc
```
{
  "customCompilers": [{
    "name": "myCompiler",
    "exe": {
      "compileObj": {
        "command": "clang++",
	"outputOption": "-o",
	"commandPrefix": "-c",
	"preprocesses": [{
	    "type": "buildIn",
	    "content": "checkUpdate" // <- build in task.
	  },{
	    "type": "terminal",
	    "content": "echo custom compiler example"
	  }
	],
	"postprocesses": [
	  ...
	]
      },
      "linkObj": {
        "command": "clang++",
	"outputOption": "-o"
      }
    },
    "static": {
      ...
    },
    "shared": {
      ...
    }
  }],
  "projects": [{
    "name": test,
    "type": "exe",
    "buildSettings": [{
      "name": "default",
      "compiler": "myCompiler", // <- specify my custom compiler
      ...
    }]
  }]
}
```

# Config File Details
## common item
- "defineVariables": define varaibles. You can't yse the variable in a variable.
- "defineTemplates": define template datas. Please refer to "TemplateItem" for the details.
```
{ // common item example
  "defineVariables": {
    "var1": "Apple",
    "var2": "Orange",
    "var3": "Grape"
  },
  "defineTemplates": [...]
}
```
## RootConfig
- "projects": define projects. Please refer to "Project" for the details.
- "customCompilers": define custom compilers. Please refer to "CustomCompiler" for the details.
```
{// RootConfig example
  "projects": [...],
  "customCompilers": [...]
}
```

## Project
- "name": project name. The variable of this inside is not evaluated.
- "template": used template data name.
- "type": project type. Please choose from among "exe", "static" or "shared".
- "buildSettings": define buildSettings. Please refer to "BuildSetting" for the details.
- "version": version number. In the case of project type is "shared", this item is used in "soname". The default value is 0.
- "minorNumber": minor number. In the case of project type is "shared", this item is used in "soname". the default value is 0.
- "releaseNumber": release number. In the case of project type is "shared", this item is used in "soname". the default value is 0.
```
{// Project example
  "name": "Project",
  "template": "",
  "type": "exe", // or "static", "shared"
  "buildSettings": [...],
  "version": 1,
  "minorNumber": 0,
  "releaseNumber": 0,
}
```

## BuildSetting
- "name": buildSetting name. The variable of this inside is not evaluated.
- "template": use template data name.
- "compiler": use command name.
- "targetDirectories": define targetDirectories. Please refer to "TargetDirectory" for the details.
- "compileOptions": add options when compile .obj.
- "includeDirectories": add include path when compile .obj. When watagashi confirm an update file to determine whether you compile it, it check the file in the pass which it appointed here.
- "linkLibraries": add link libraries.
- "libraryDirectories": add link library path.
- "linkOptions": add options when link .obj's.
- "dependences": dependence project. Please write it as follows, "\<project name\>.\<buildSetting name\>".
- "outputConfig": define OutputConfig. Please refer to "OutputConfig" for the details.
- "preprocess": specify a preprocess command at build.
- "linkPreprocess": specify a link preprocess command at build.
- "postprocess": specify a postprocess command at build.
```
{// BuildSetting example
  "name": "default",
  "template": "",
  "compiler": "clang++",
  "dependences": ["otherProject.release"]
  "targetDirectories": [...],
  "compileOptions": ["-O3"],
  "includeDirectories": ["includePath"],
  "linkLibraries": ["boost_system"],
  "libraryDirectories": ["BoostLibPath"],
  "linkOptions": [""],
  "outputConfig": {...},
  "preprocess": "echo Hello Watagashi!",
  "linkPreprocess": "echo Hello Watagashi!!",
  "postprocess": "echo Hello Watagashi!!!"
}
```

## OutputConfig
- "name": output file name. When project type is "shared", this name is library name.
- "outputPath": specify directory of output files.
- "intermediatePath": specify directory of intermeidate files (.obj).
```
{// OutputConfig example
  "name": "Apple",
  "outputPath": "Oragne",
  "intermediatePath": "Grape"
}
```

## TargetDirectory
- "path": target directory. This item can use regular expression.
- "template": use template data name.
- "fileFilters": define fileFilters. Please refer to "FileFilter" for the details.
- "ignores": add ignored files. This item can use regular expression. When you ignore the direcoty, please touch '/' at the end.
```
{// TargetDirectory example
  "path": "src",
  "template": "",
  "fileFilters": [...],
  "ignores": ["dummy.cpp", "dummyDir/"]
}
```

## FileFilter
- "template": use template data name.
- "extensions": filter extensions. This item can use regular expression.
- "filesToProcess": define filesToProcesses. Please refer to "FileToProcess" for the details.
```
{// FileFilter example
  "template": "",
  "extensions": ["cpp", "cxx"],
  "filesToProcess": [...]
}
```

## FileToProcess
- "filepath": target filepath.
- "template": use template data name.
- "compileOptions": add options when compile .obj.
- "preprocess": specify a preprocess command at build.
- "postprocess": specify a postprocess command at build.
```
{// FilToProcess example
  "filepath": "file.cpp",
  "template": ""
  "compileOptions": ["-Od"],
  "preprocess": "echo Let's file to preprocess.",
  "postprocess": "echo Let's file to postprocess.",
}
```

## TemplateItem
- "name": buildSetting name. The variable of this inside is not evaluated.
- "itemType": template data type. Please choose from among "project", "buildSetting", "targetDirectory", "fileFilter", "fileToProcess", "customCompiler", "compileTask", "compileTaskGroup", "compileProcess", or "shared".
```
{// TemplateItem example
  "name": "common",
  "itemType": "targetDirectory",
  "path": "src",
  ...
}
```

## CustomCompiler 
- "name": custom compiler name. The variable of this inside is not evaluated.
- "exe": define compileTaskGroup when project type is "exe". Please refer to "CompileTaskGroup" for the details.
- "static": define compileTaskGroup when project type is "static". Please refer to "CompileTaskGroup" for the details.
- "shared": define compileTaskGroup when project type is "shared". Please refer to "CompileTaskGroup" for the details.
```
{// CustomCompiler example
  "name": "customCompiler",
  "exe": {...},
  "static": {...},
  "shared": {...}
}
```

## CompileTaskGroup
- "compileObj": define compileTask to compile obj file. Please refer to "CompileTask" for the details.
- "linkObj": define compileTask to link obj files. Please refer to "CompileTask" for the details.
```
{// CompileTaskGroup example
  "compileObj": {...},
  "linkObj": {...}
}
```

## CompileTask
- "command": use command name.
- "inputOption": specify input file option.
- "outputOption": specify output file option.
- "commandPrefix": specify prefix of option.
- "commandSuffix": specify suffix of option.
- "preprocesses": define compileProcess to preprocess. Please refer to "CompileProcess" for the details.
- "postprocesses": define compileProcess to postprocess. Please refer to "CompileProcess" for the details.
```
{// CompileTask example -> clang++ -c \<input filepath/> -o <output filepath/> -Od
  "command": "clang++",
  "inputOption": "",
  "outputOption": "-o"
  "commandPrefix": "-c",
  "commandSuffix": "-Od",
  "preprocesses": {...},
  "postprocesses": {...}
}
```

## CompileProcess
- "type": process type. Please choose from among "terminal", or "buildIn".
- "content": content of process. In the case type is "buildIn", this item can appoint "checkUpdate".
```
{
  "type": "terminal",
  "content": "echo custom complier process."
}
```
