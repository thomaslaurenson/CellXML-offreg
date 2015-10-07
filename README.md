# CellXML
CellXML.exe is a simple portable Windows application that parses the binary structure of an offline Windows Registry hive file and converts it to an XML format. The output XML structure is based on [RegXML](http://forensicswiki.org/wiki/RegXML "Forensic Wiki"). 

## CellXML Usage Examples

The general usage format for CellXML is:

`CellXML.exe [options] hive-file`

The following list provides some examples of CellXML usage:

1. Print Registry hive file to standard output (stdout):
  * `CellXML.exe hive-file`
2. Manually specify the hive root key:
  * `CellXML.exe -r $$$PROTO.HIV hive-file`
3. Automatically determine hive root key (experimental):
  * `CellXML.exe -a hive-file`
4. Direct standard output to an XML file:
  * `CellXML.exe hive-file > output.xml`

## Offline Registry Library and offreg.dll

CellXML uses the Offline Registry Library (offreg.dll) to aid in parsing the binary structure of the Windows Registry. According to [Microsoft](https://msdn.microsoft.com/en-us/library/ee210757%28v=vs.85%29.aspx "Microsoft") the offline registry library (offreg.dll) is used to modify a registry hive outside the active system registry. 

The offline registry library is provided as a binary redistributable dynamic-link library (DLL). The offreg.dll is provided in this repository for 32 bit and 64 bit architectures.

## Windows Operating System Support

CellXML has been tested on Microsoft Windows 7 (32-bit). Testing has not been performed on any other Windows versions. According to [Microsoft](https://msdn.microsoft.com/en-us/library/ee210757%28v=vs.85%29.aspx "Microsoft") the library runs on the following versions of Windows: 

1. Windows XP
2. Windows Server 2003
3. Windows Vista
4. Windows 7
5. Windows Server 2008
6. Windows Server 2008 R2

It is probable that newer versions of Microsoft Windows (8, 8.1 and 10) may support offreg.dll and, therefore, run CellXML. However, no other versions of Microsoft Windows has been tested.

## Windows Registry Hive File Support

According to [Microsoft](https://msdn.microsoft.com/en-us/library/ee210757%28v=vs.85%29.aspx "Microsoft") the offline registry library (offreg.dll) supports registry hive formats starting with Windows XP. Therefore, CellXML **does not support** processing offline Registry hive files taken from versions of Microsoft Windows **prior to Windows XP**.

## Sample Registry Hive Files

Three hive files are distributed with the CellXML project source code and can be found in the **sample-hives** folder. The sample hive files are included to provide testing data. The hive files have been taken from the [Digital Corpora](http://digitalcorpora.org/ "Digital Corpora") website, specifically the [m57 Patents](http://digitalcorpora.org/corp/nps/scenarios/2009-m57-patents/ "Digital Corpora") scenario. One forensic image, [charlie-2009-11-12start.E01](http://digitalcorpora.org/corp/nps/scenarios/2009-m57-patents/drives-redacted/charlie-2009-11-12start.E01 "charlie-2009-11-12start.E01"), was processed and the three hive files extracted using the [HiveExtractor.py](https://github.com/thomaslaurenson/DFXMLTools/tree/master/HiveExtractor "HiveExtractor.py") tool. The following hive files are from a Windows XP system:

| Hive file   | MD5 hash value                   |
| ------------|----------------------------------|
| NTUSER.DAT  | aeabc1b46b86cdc7ed0ac6598ff2bdb0 |
| SOFTWARE    | c76de38c5e5557f59078b5317705fba5 |
| SYSTEM      | 151ebb9b38cf06f018c4ed1fdf69e2de |

#### License Details:

The [m57 Patents license](http://digitalcorpora.org/corp/nps/scenarios/2009-m57-patents/docs/License "m57 Patents license") specifies the following conditions:

> Contains information derived from copyrighted materials. For use only for research, education, training only, and the production of educational materials. All other uses require the permission of the copyright holders.

## Compiling Intructions

This software is authored using Microsoft Visual Studio 2015. The Visual Studio Studio Solution file (CellXML.sln) is located in the root directory of the project. 

## Limitations

CellXML is known to have the following limitations: 

1. Cannot report hive offset for Registry keys or values (this is a known limitation when using offreg.dll).
2. Cannot parse Registry hive files from systems earlier than Windows XP
