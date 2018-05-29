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

## Advance Usage

# Regular Expression
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

# List Up Task
You display files for the build by using "listup" task.
```
watagashi -p test listup
```
And, you can confirm files for the build when you used the regular expression if you use "check-regex" option.
```
watagashi -p test listup --check-regex \d+\.cpp
```

# Process
Watagasi execute to preprocess before compile source files, preprocess before link or postprocess.
Please describle string to express a command in "preprocess", "linkPreporcess" or "postprocess" of "BuildSetting" to set it.
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

# Dependence Relationship Between Project
Watagashi can appoint dependence by describing "<project name>.<buildSetting name>" in "dependences" of "BuildSetting".
When build project, it build dependence project earlier if a project of the dependence is non-update.
```
"buildSettings": [{
	...
	"dependences": ["other_project.default"],
	...
}]
```

# Variable
# Template
