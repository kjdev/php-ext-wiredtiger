# PHP Extension for WiredTiger

Fast transactional storage - a PHP wrapper around the
[WiredTiger](http://wiredtiger.com) storage engine library.

## Build

Required install WiredTiger.

```
% phpize
% ./configure
% make
% make install
```

### Build option

* --with-wiredtiger-includedir

  WiredTiger include directory.
  (ex: /usr/include)

* --with-wiredtiger-libdir

  WiredTiger library (libwiredtiger.so) directory.
  (ex: /usr/lib64)

## Configration

wiredtiger.ini:

```
extension=wiredtiger.so
```

## Class synopsis

```
WiredTiger\Db {
  public __construct(string $home, string $config)
  public bool create(string $uri, string $config = "key_format=S,value_format=S")
  public WiredTiger\Cursor open(string $uri, string $config = NULL)
  public bool close(void)
  public bool drop(string $uri)
}
```

```
WiredTiger\Cursor {
  public __construct(WiredTiger\Db $db, string $uri, string $config = NULL)
  public string get(string $key)
  public bool set(string $key, string $value)
  public bool remove(string $key)
  public bool close(void)
  public string current(void)
  public string key(void)
  public bool next(void)
  public bool rewind(void)
  public bool valid(void)
  public bool prev(void)
  public bool last(void)
  public bool seek(string $key, bool $near = FALSE)
}
```

## Examples

``` php
namespace WiredTiger;

$home = __DIR__ . '/wt';
$uri = 'table:example';

// Create database
$db = new Db($home);
$db->create($uri);

// Open table (uri)
$cursor = $db->open($uri);

// Set data
$cursor->set('key', 'value');

// Get data
$data = $cursor->get('key');

// Itetator
foreach ($cursor as $key => $val) {
    echo "{$key} => {$val}\n";
}
```
