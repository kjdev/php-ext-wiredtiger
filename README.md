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
  public string get(mixed $key)
  public bool set(mixed $key, mixed $value [, ... ])
  public bool remove(mixed $key)
  public bool close(void)
  public string current(void)
  public string key(void)
  public bool next(void)
  public bool rewind(void)
  public bool valid(void)
  public bool prev(void)
  public bool last(void)
  public bool seek(mixed $key, bool $near = FALSE)
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

### Setting value and key

* Value

```
$db->create($uri, 'value_format=SS');

$cursor->set('key', 'value-1', 'value-2');
  // or
$cursor->set('key', ['value-1', 'value-2']);
```

* Key

```
$db->create($uri, 'key_format=iS');

$cursor->set([1, 'key-1'], 'value-1');
```

* Key autoincrement

```
$db->create($uri, 'key_format=r'); // Set config: key_format=r
$cursor = $db->open($uri, 'append'); // Set config: append

$cursor->set(0, 'value-1'); // Set key: 0
$cursor->set(0, 'value-2');
$cursor->set(0, 'value-3');

foreach ($cursor as $key => $val) {
    echo "{$key}: {$val}\n";
}

/*
Output:
  1: value-1
  2: value-2
  3: value-3
*/
```
