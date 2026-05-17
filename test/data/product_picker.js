(function(root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.ProductPicker = factory();
  }
}(typeof self !== 'undefined' ? self : this, function() {
  function normalizeText(value) {
    return String(value || '').toLowerCase().trim();
  }

  function getProductId(product) {
    return String(product && product.id !== undefined ? product.id : '');
  }

  function getSelectedProductIds(selects, currentValue) {
    const selectedIds = new Set();
    const currentId = String(currentValue || '');

    Array.from(selects || []).forEach(select => {
      const value = String(select && select.value ? select.value : '');
      if (value && value !== currentId) {
        selectedIds.add(value);
      }
    });

    return selectedIds;
  }

  function productMatches(product, query, getDisplayText) {
    const normalizedQuery = normalizeText(query);
    if (!normalizedQuery) return true;

    const fields = [
      product && product.code,
      product && product.name,
      product && product.group,
      typeof getDisplayText === 'function' ? getDisplayText(product) : ''
    ].map(normalizeText);

    return fields.some(field => field.startsWith(normalizedQuery));
  }

  function getProductPickerPage(options) {
    const products = Array.isArray(options && options.products) ? options.products : [];
    const selectedIds = options && options.selectedIds instanceof Set ? options.selectedIds : new Set();
    const currentValue = String(options && options.currentValue ? options.currentValue : '');
    const pageSize = Math.max(1, Number(options && options.pageSize) || 5);
    const requestedPage = Math.max(1, Number(options && options.page) || 1);
    const query = options && options.query;
    const getDisplayText = options && options.getDisplayText;

    const filtered = products.filter(product => {
      const productId = getProductId(product);
      if (!productId) return false;
      if (selectedIds.has(productId) && productId !== currentValue) return false;
      return productMatches(product, query, getDisplayText);
    });

    const totalItems = filtered.length;
    const totalPages = Math.max(1, Math.ceil(totalItems / pageSize));
    const page = Math.min(requestedPage, totalPages);
    const start = (page - 1) * pageSize;

    return {
      items: filtered.slice(start, start + pageSize),
      page,
      pageSize,
      totalItems,
      totalPages
    };
  }

  return {
    getSelectedProductIds,
    getProductPickerPage
  };
}));
