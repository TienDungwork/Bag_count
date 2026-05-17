const assert = require('assert');

const {
  getSelectedProductIds,
  getProductPickerPage
} = require('./data/product_picker');

const products = [
  { id: 1, code: 'XM001', name: 'Xi mang PCB40', group: 'Xi mang' },
  { id: 2, code: 'XM002', name: 'Xi mang PCB30', group: 'Xi mang' },
  { id: 3, code: 'CAT001', name: 'Cat vang', group: 'Cat' },
  { id: 4, code: 'DA001', name: 'Da 1x2', group: 'Da' },
  { id: 5, code: 'THEP01', name: 'Thep cuon', group: 'Thep' },
  { id: 6, code: 'GACH01', name: 'Gach do', group: 'Gach' },
  { id: 7, code: 'GACH02', name: 'Gach block', group: 'Gach' }
];

const selectedIds = getSelectedProductIds(
  [
    { value: '1' },
    { value: '3' },
    { value: '7' }
  ],
  '3'
);

assert.deepStrictEqual(selectedIds, new Set(['1', '7']));

const filtered = getProductPickerPage({
  products,
  query: 'gach',
  selectedIds,
  currentValue: '7',
  page: 1,
  pageSize: 5,
  getDisplayText: product => `${product.group} - ${product.code} - ${product.name}`
});

assert.strictEqual(filtered.totalItems, 2);
assert.strictEqual(filtered.totalPages, 1);
assert.deepStrictEqual(filtered.items.map(product => product.id), [6, 7]);

const firstPage = getProductPickerPage({
  products,
  query: '',
  selectedIds: new Set(['1']),
  currentValue: '',
  page: 1,
  pageSize: 5,
  getDisplayText: product => `${product.group} - ${product.code} - ${product.name}`
});

assert.strictEqual(firstPage.totalItems, 6);
assert.strictEqual(firstPage.totalPages, 2);
assert.strictEqual(firstPage.items.length, 5);
assert.ok(!firstPage.items.some(product => product.id === 1));

const prefixOnly = getProductPickerPage({
  products: [
    { id: 8, code: '1001', name: 'Bao 25kg', group: 'Hang bao' },
    { id: 9, code: 'XM1001', name: 'Xi mang 1001', group: 'Xi mang' },
    { id: 10, code: '2001', name: 'Loai 1 dac biet', group: 'Hang bao' }
  ],
  query: '1',
  selectedIds: new Set(),
  currentValue: '',
  page: 1,
  pageSize: 5,
  getDisplayText: product => `${product.group} - ${product.code} - ${product.name}`
});

assert.deepStrictEqual(prefixOnly.items.map(product => product.id), [8]);

const selectedCurrentIsExcludedWhileSearching = getProductPickerPage({
  products: [
    { id: 11, code: '001', name: '001 selected product', group: 'A' },
    { id: 12, code: '001-NEW', name: '001 new product', group: 'A' }
  ],
  query: '001',
  selectedIds: new Set(['11']),
  currentValue: '',
  page: 1,
  pageSize: 5,
  getDisplayText: product => `${product.group} - ${product.code} - ${product.name}`
});

assert.deepStrictEqual(selectedCurrentIsExcludedWhileSearching.items.map(product => product.id), [12]);

console.log('product picker tests passed');
