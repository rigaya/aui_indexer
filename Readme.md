
# aui_indexer  
by rigaya  

lwinput.auiやm2v.auiで必要なindexファイルなどを事前生成します。  
インデックス生成は非常に時間がかかるため、一気に複数のファイルのインデックスファイルを更新したい場合に使用したり、録画後バッチ処理などで使用し、動画をAviutlに瞬時に読み込めるようにします。

## 配布場所 & 更新履歴  
[rigayaの日記兼メモ帳＞＞](http://rigaya34589.blog135.fc2.com/blog-category-22.html)  

## 基本動作環境  
Windows 7, 8, 8.1, 10 (x86/x64)  
Aviutl 0.99m 以降

## aui_indexerの使用方法
```bat
aui_indexer.exe [オプション] <対象ファイル1> [<対象ファイル2>] [] ...
```
対象ファイルすべてに対してインデックス生成を行います。

### オプション
```
-lwtmpdir <string>
```
指定したフォルダにlwiファイル(lwinput.auiのインデックスファイル)をいったん出力し、インデックス生成完了後、本来のパスに移動します。  
読み込み元と異なるドライブのフォルダを指定することで、インデックス生成の高速化が期待できるかもしれません。

```
-aui <string>
```
使用するauiのパスを指定します。
-auiには任意のauiを指定できるはず…です。
lwinput.auiとm2v.auiでしか試してませんが。

-auiを指定しない場合、
- lwinput.aui
- plugins\lwinput.aui
- m2v.aui
- plugins\m2v.aui
- lsmashinput.aui
- plugins\lsmashinput.aui  
を上から順に検索し、使用します。

make_glのようにレジストリから拾っているわけではないので、
上記以外にauiがある場合には-auiを指定してください。

## aui_indexer 使用にあたっての注意事項  
無保証です。自己責任で使用してください。   
NVEncを使用したことによる、いかなる損害・トラブルについても責任を負いません。  


### aui_indexerのソースコードについて
MITライセンスです。

### ソースの構成
Windows ... VCビルド  

文字コード: UTF-8-BOM  
改行: CRLF  
インデント: 空白x4  
